/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef EXAMPLE_PLATFORM_LINUX_CAN_MEDIA_HPP_INCLUDED
#define EXAMPLE_PLATFORM_LINUX_CAN_MEDIA_HPP_INCLUDED

#include "../../posix/posix_executor_extension.hpp"
#include "../../posix/posix_platform_error.hpp"
#include "socketcan.h"

#include <canard.h>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>
#include <cetl/rtti.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/transport/can/media.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/types.hpp>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace example
{
namespace platform
{
// Can't use lowercased `linux` - gnuc++ defines it as macro.
namespace Linux
{

class CanMedia final : public libcyphal::transport::can::IMedia
{
public:
    struct Collection
    {
        Collection() = default;

        bool make(libcyphal::IExecutor& executor, std::vector<std::string>& iface_addresses)
        {
            reset();

            for (const auto& iface_address : iface_addresses)
            {
                auto maybe_media = CanMedia::make(executor, iface_address);
                if (auto* const error = cetl::get_if<libcyphal::transport::PlatformError>(&maybe_media))
                {
                    std::cerr << "Failed to create CAN media '" << iface_address << "', errno=" << (*error)->code()
                              << ".";
                    return false;
                }
                media_vector_.emplace_back(cetl::get<Linux::CanMedia>(std::move(maybe_media)));
            }

            for (auto& media : media_vector_)
            {
                media_ifaces_.push_back(&media);
            }

            return true;
        }

        cetl::span<IMedia*> span()
        {
            return {media_ifaces_.data(), media_ifaces_.size()};
        }

        void reset()
        {
            media_vector_.clear();
            media_ifaces_.clear();
        }

    private:
        std::vector<CanMedia> media_vector_;
        std::vector<IMedia*>  media_ifaces_;

    };  // Collection

    CETL_NODISCARD static cetl::variant<CanMedia, libcyphal::transport::PlatformError> make(
        libcyphal::IExecutor& executor,
        const std::string&    iface_address)
    {
        const SocketCANFD socket_can_rx_fd = ::socketcanOpen(iface_address.c_str(), false);
        if (socket_can_rx_fd < 0)
        {
            return libcyphal::transport::PlatformError{posix::PosixPlatformError{-socket_can_rx_fd}};
        }

        // We gonna register separate callbacks for rx & tx (aka pop & push),
        // so at executor (especially in case of the "epoll" one) we need separate file descriptors.
        const SocketCANFD socket_can_tx_fd = ::dup(socket_can_rx_fd);
        if (socket_can_tx_fd == -1)
        {
            const int error_code = errno;
            (void) ::close(socket_can_rx_fd);
            return libcyphal::transport::PlatformError{posix::PosixPlatformError{error_code}};
        }

        return CanMedia{executor, socket_can_rx_fd, socket_can_tx_fd, iface_address};
    }

    ~CanMedia()
    {
        if (socket_can_rx_fd_ >= 0)
        {
            (void) ::close(socket_can_rx_fd_);
        }
        if (socket_can_tx_fd_ >= 0)
        {
            (void) ::close(socket_can_tx_fd_);
        }
    }

    CanMedia(const CanMedia&)            = delete;
    CanMedia& operator=(const CanMedia&) = delete;

    CanMedia(CanMedia&& other) noexcept
        : executor_{other.executor_}
        , socket_can_rx_fd_{std::exchange(other.socket_can_rx_fd_, -1)}
        , socket_can_tx_fd_{std::exchange(other.socket_can_tx_fd_, -1)}
        , iface_address_{other.iface_address_}
    {
    }
    CanMedia* operator=(CanMedia&&) noexcept = delete;

    void tryReopen()
    {
        if (socket_can_rx_fd_ >= 0)
        {
            (void) ::close(socket_can_rx_fd_);
            socket_can_rx_fd_ = -1;
        }
        if (socket_can_tx_fd_ >= 0)
        {
            (void) ::close(socket_can_tx_fd_);
            socket_can_tx_fd_ = -1;
        }

        const SocketCANFD socket_can_rx_fd = ::socketcanOpen(iface_address_.c_str(), false);
        if (socket_can_rx_fd >= 0)
        {
            socket_can_rx_fd_ = socket_can_rx_fd;
        }
        socket_can_tx_fd_ = ::dup(socket_can_rx_fd);
    }

private:
    using Filter  = libcyphal::transport::can::Filter;
    using Filters = libcyphal::transport::can::Filters;

    CanMedia(libcyphal::IExecutor& executor,
             const SocketCANFD     socket_can_rx_fd,
             const SocketCANFD     socket_can_tx_fd,
             std::string           iface_address)
        : executor_{executor}
        , socket_can_rx_fd_{socket_can_rx_fd}
        , socket_can_tx_fd_{socket_can_tx_fd}
        , iface_address_{std::move(iface_address)}
    {
    }

    CETL_NODISCARD libcyphal::IExecutor::Callback::Any registerAwaitableCallback(
        libcyphal::IExecutor::Callback::Function&&              function,
        const posix::IPosixExecutorExtension::Trigger::Variant& trigger) const
    {
        auto* const posix_executor_ext = cetl::rtti_cast<posix::IPosixExecutorExtension*>(&executor_);
        if (nullptr == posix_executor_ext)
        {
            return {};
        }

        return posix_executor_ext->registerAwaitableCallback(std::move(function), trigger);
    }

    // MARK: - IMedia

    std::size_t getMtu() const noexcept override
    {
        return CANARD_MTU_CAN_CLASSIC;
    }

    cetl::optional<libcyphal::transport::MediaFailure> setFilters(const Filters filters) noexcept override
    {
        std::vector<CanardFilter> can_filters;
        can_filters.reserve(filters.size());
        std::transform(filters.begin(), filters.end(), std::back_inserter(can_filters), [](const Filter filter) {
            return CanardFilter{filter.id, filter.mask};
        });

        const std::int16_t result = ::socketcanFilter(socket_can_rx_fd_, can_filters.size(), can_filters.data());
        if (result < 0)
        {
            return libcyphal::transport::PlatformError{posix::PosixPlatformError{-result}};
        }
        return cetl::nullopt;
    }

    PushResult::Type push(const libcyphal::TimePoint /* deadline */,
                          const libcyphal::transport::can::CanId can_id,
                          const cetl::span<const cetl::byte>     payload) noexcept override
    {
        const CanardFrame  canard_frame{can_id, payload.size(), payload.data()};
        const std::int16_t result = ::socketcanPush(socket_can_tx_fd_, &canard_frame, 0);
        if (result < 0)
        {
            return libcyphal::transport::PlatformError{posix::PosixPlatformError{-result}};
        }

        const bool is_accepted = result > 0;
        return PushResult::Success{is_accepted};
    }

    CETL_NODISCARD PopResult::Type pop(const cetl::span<cetl::byte> payload_buffer) noexcept override
    {
        CanardFrame canard_frame{};
        bool        is_loopback{false};

        const std::int16_t result = ::socketcanPop(socket_can_rx_fd_,
                                                   &canard_frame,
                                                   nullptr,
                                                   payload_buffer.size(),
                                                   payload_buffer.data(),
                                                   0,
                                                   &is_loopback);
        if (result < 0)
        {
            return libcyphal::transport::PlatformError{posix::PosixPlatformError{-result}};
        }
        if (result == 0)
        {
            return cetl::nullopt;
        }

        return PopResult::Metadata{executor_.now(), canard_frame.extended_can_id, canard_frame.payload_size};
    }

    CETL_NODISCARD libcyphal::IExecutor::Callback::Any registerPushCallback(
        libcyphal::IExecutor::Callback::Function&& function) override
    {
        using WritableTrigger = posix::IPosixExecutorExtension::Trigger::Writable;
        return registerAwaitableCallback(std::move(function), WritableTrigger{socket_can_tx_fd_});
    }

    CETL_NODISCARD libcyphal::IExecutor::Callback::Any registerPopCallback(
        libcyphal::IExecutor::Callback::Function&& function) override
    {
        using ReadableTrigger = posix::IPosixExecutorExtension::Trigger::Readable;
        return registerAwaitableCallback(std::move(function), ReadableTrigger{socket_can_rx_fd_});
    }

    // MARK: Data members:

    libcyphal::IExecutor& executor_;
    SocketCANFD           socket_can_rx_fd_;
    SocketCANFD           socket_can_tx_fd_;
    const std::string     iface_address_;

};  // CanMedia

}  // namespace Linux
}  // namespace platform
}  // namespace example

#endif  // EXAMPLE_PLATFORM_LINUX_CAN_MEDIA_HPP_INCLUDED
