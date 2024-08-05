/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef EXAMPLE_PLATFORM_LINUX_CAN_MEDIA_HPP_INCLUDED
#define EXAMPLE_PLATFORM_LINUX_CAN_MEDIA_HPP_INCLUDED

#include "socketcan.h"

#include <canard.h>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/transport/can/media.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/types.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <ratio>
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
    CETL_NODISCARD static cetl::variant<CanMedia, libcyphal::transport::PlatformError> make(
        cetl::pmr::memory_resource& memory,
        libcyphal::IExecutor&       executor,
        const std::string&          iface_address)
    {
        const SocketCANFD socket_can_fd = ::socketcanOpen(iface_address.c_str(), false);
        if (socket_can_fd < 0)
        {
            return libcyphal::transport::PlatformError{posix::PosixPlatformError{-socket_can_fd}};
        }

        return CanMedia{memory, executor, socket_can_fd};
    }

    ~CanMedia()
    {
        (void) ::close(socket_can_fd_);
    }

    CanMedia(const CanMedia&)            = delete;
    CanMedia& operator=(const CanMedia&) = delete;

    CanMedia(CanMedia&& other) noexcept
        : memory_{other.memory_}
        , executor_{other.executor_}
        , socket_can_fd_{std::exchange(other.socket_can_fd_, -1)}
    {
    }

    CanMedia& operator=(CanMedia&& other) noexcept
    {
        memory_        = other.memory_;
        executor_      = other.executor_;
        socket_can_fd_ = std::exchange(other.socket_can_fd_, -1);

        return *this;
    }

private:
    using Filter  = libcyphal::transport::can::Filter;
    using Filters = libcyphal::transport::can::Filters;

    CanMedia(cetl::pmr::memory_resource& memory, libcyphal::IExecutor& executor, SocketCANFD socket_can_fd)
        : memory_{memory}
        , executor_{executor}
        , socket_can_fd_{socket_can_fd}
    {
    }

    CETL_NODISCARD static libcyphal::IExecutor::Callback::Any registerCallbackWithCondition(
        libcyphal::IExecutor&                                         executor,
        libcyphal::IExecutor::Callback::Function&&                    function,
        const posix::IPosixExecutorExtension::WhenCondition::Variant& condition)
    {
        auto* const posix_extension = cetl::rtti_cast<posix::IPosixExecutorExtension*>(&executor);
        if (nullptr == posix_extension)
        {
            return {};
        }

        auto callback = executor.registerCallback(std::move(function));
        posix_extension->scheduleCallbackWhen(callback, condition);
        return callback;
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

        const std::int16_t result = ::socketcanFilter(socket_can_fd_, can_filters.size(), can_filters.data());
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
        const std::int16_t result = ::socketcanPush(socket_can_fd_, &canard_frame, 0);
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

        const std::int16_t result = ::socketcanPop(socket_can_fd_,
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

        return PopResult::Metadata{executor_.get().now(), canard_frame.extended_can_id, canard_frame.payload_size};
    }

    CETL_NODISCARD libcyphal::IExecutor::Callback::Any registerPushCallback(
        libcyphal::IExecutor&                      executor,
        libcyphal::IExecutor::Callback::Function&& function) override
    {
        using HandleWritable = posix::IPosixExecutorExtension::WhenCondition::HandleWritable;
        return registerCallbackWithCondition(executor, std::move(function), HandleWritable{socket_can_fd_});
    }

    CETL_NODISCARD libcyphal::IExecutor::Callback::Any registerPopCallback(
        libcyphal::IExecutor&                      executor,
        libcyphal::IExecutor::Callback::Function&& function) override
    {
        using HandleReadable = posix::IPosixExecutorExtension::WhenCondition::HandleReadable;
        return registerCallbackWithCondition(executor, std::move(function), HandleReadable{socket_can_fd_});
    }

    // MARK: Data members:

    cetl::pmr::memory_resource&                  memory_;
    std::reference_wrapper<libcyphal::IExecutor> executor_;
    SocketCANFD                                  socket_can_fd_;

};  // CanMedia

}  // namespace Linux
}  // namespace platform
}  // namespace example

#endif  // EXAMPLE_PLATFORM_LINUX_CAN_MEDIA_HPP_INCLUDED
