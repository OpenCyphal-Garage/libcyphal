/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef EXAMPLE_PLATFORM_POSIX_POLL_SINGLE_THREADED_EXECUTOR_HPP_INCLUDED
#define EXAMPLE_PLATFORM_POSIX_POLL_SINGLE_THREADED_EXECUTOR_HPP_INCLUDED

#include "posix_executor_extension.hpp"
#include "posix_platform_error.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/rtti.hpp>
#include <cetl/variable_length_array.hpp>
#include <cetl/visit_helpers.hpp>
#include <libcyphal/errors.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/platform/single_threaded_executor.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/types.hpp>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sys/poll.h>
#include <thread>

namespace example
{
namespace platform
{
namespace posix
{

/// @brief Defines POSIX platform specific single-threaded executor based on `poll` mechanism.
///
class PollSingleThreadedExecutor final : public libcyphal::platform::SingleThreadedExecutor,
                                         public IPosixExecutorExtension
{
public:
    explicit PollSingleThreadedExecutor(cetl::pmr::memory_resource& memory_resource)
        : awaitable_nodes_{&awaitable_nodes_, &awaitable_nodes_}
        , total_awaitables_{0}
        , poll_fds_{&memory_resource}
        , callback_interfaces_{&memory_resource}
    {
    }
    ~PollSingleThreadedExecutor() override = default;

    PollSingleThreadedExecutor(const PollSingleThreadedExecutor&)                = delete;
    PollSingleThreadedExecutor(PollSingleThreadedExecutor&&) noexcept            = delete;
    PollSingleThreadedExecutor& operator=(const PollSingleThreadedExecutor&)     = delete;
    PollSingleThreadedExecutor& operator=(PollSingleThreadedExecutor&&) noexcept = delete;

    using PollFailure =
        cetl::variant<libcyphal::MemoryError, libcyphal::transport::PlatformError, libcyphal::ArgumentError>;

    cetl::optional<PollFailure> pollAwaitableResourcesFor(const cetl::optional<libcyphal::Duration> timeout)
    {
        CETL_DEBUG_ASSERT((total_awaitables_ > 0) || timeout,
                          "Infinite timeout without awaitables means that we will sleep forever.");

        if (total_awaitables_ == 0)
        {
            if (!timeout)
            {
                return libcyphal::ArgumentError{};
            }

            std::this_thread::sleep_for(*timeout);
            return cetl::nullopt;
        }

        // (Re-)populate the poll file descriptors and the callback IDs into variable size arrays.
        // Note, their `clear` doesn't deallocate the memory,
        // so we can reuse them, and grow on demand (but never shrink).
        //
        poll_fds_.clear();
        callback_interfaces_.clear();
        for (auto* node = awaitable_nodes_.next_node; node != &awaitable_nodes_; node = node->next_node)
        {
            auto& awaitable_node = static_cast<AwaitableNode&>(*node);
            callback_interfaces_.push_back(&awaitable_node);
            poll_fds_.push_back({awaitable_node.fd(), static_cast<std::int16_t>(awaitable_node.events()), 0});
        }
        if ((total_awaitables_ != poll_fds_.size()) || (total_awaitables_ != callback_interfaces_.size()))
        {
            return libcyphal::MemoryError{};
        }

        // Make sure that timeout is within the range of `::poll()`'s `int` timeout parameter.
        // Any possible negative timeout will be treated as zero (return immediately from the `::poll`).
        //
        int clamped_timeout_ms = -1;  // "infinite" timeout
        if (timeout)
        {
            using PollDuration = std::chrono::milliseconds;

            clamped_timeout_ms = static_cast<int>(  //
                std::max(static_cast<PollDuration::rep>(0),
                         std::min(std::chrono::duration_cast<PollDuration>(*timeout).count(),
                                  static_cast<PollDuration::rep>(std::numeric_limits<int>::max()))));
        }

        int poll_result = ::poll(poll_fds_.data(), static_cast<nfds_t>(poll_fds_.size()), clamped_timeout_ms);
        if (poll_result < 0)
        {
            const auto err = errno;
            return libcyphal::transport::PlatformError{PosixPlatformError{err}};
        }
        if (poll_result == 0)
        {
            return cetl::nullopt;
        }

        const auto now_time = now();
        for (std::size_t index = 0; (index < poll_fds_.size()) && (poll_result > 0); ++index)
        {
            const pollfd& poll_fd = poll_fds_[index];
            if (poll_fd.revents != 0)
            {
                // Allows to leave "earlier" from the loop if we have no more events to process.
                --poll_result;

                if (0 != (static_cast<PollEvents>(poll_fd.revents) & static_cast<PollEvents>(poll_fd.events)))
                {
                    if (auto* const cb_interface = callback_interfaces_[index])
                    {
                        cb_interface->schedule(Callback::Schedule::Once{now_time});
                    }
                }
            }
        }

        return cetl::nullopt;
    }

    /// @brief Releases temporary resources.
    ///
    /// In use for testing purposes only, namely so that tracking memory resource
    /// won't report these temporary allocations as memory leaks.
    ///
    void releaseTemporaryResources()
    {
        poll_fds_.clear();
        poll_fds_.shrink_to_fit();

        callback_interfaces_.clear();
        callback_interfaces_.shrink_to_fit();
    }

protected:
    // MARK: - IPosixExecutorExtension

    CETL_NODISCARD Callback::Any registerAwaitableCallback(Callback::Function&&    function,
                                                           const Trigger::Variant& trigger) override
    {
        AwaitableNode new_cb_node{*this, std::move(function), awaitable_nodes_};

        cetl::visit(  //
            cetl::make_overloaded(
                [&new_cb_node](const Trigger::Readable& readable) {
                    //
                    new_cb_node.setup(readable.fd, POLLIN);
                },
                [&new_cb_node](const Trigger::Writable& writable) {
                    //
                    new_cb_node.setup(writable.fd, POLLOUT);
                }),
            trigger);

        insertCallbackNode(new_cb_node);
        return {std::move(new_cb_node)};
    }

    // MARK: - RTTI

    CETL_NODISCARD void* _cast_(const cetl::type_id& id) & noexcept override
    {
        if (id == IPosixExecutorExtension::_get_type_id_())
        {
            return static_cast<IPosixExecutorExtension*>(this);
        }
        return Base::_cast_(id);
    }
    CETL_NODISCARD const void* _cast_(const cetl::type_id& id) const& noexcept override
    {
        if (id == IPosixExecutorExtension::_get_type_id_())
        {
            return static_cast<const IPosixExecutorExtension*>(this);
        }
        return Base::_cast_(id);
    }

private:
    using PollEvents = std::uint16_t;
    using Base       = SingleThreadedExecutor;
    using Self       = PollSingleThreadedExecutor;

    struct DoubleLinkedNode
    {
        DoubleLinkedNode* prev_node;
        DoubleLinkedNode* next_node;
    };

    class AwaitableNode final : public CallbackNode, public DoubleLinkedNode
    {
    public:
        AwaitableNode(Self& executor, Callback::Function&& function, DoubleLinkedNode& origin_node)
            : CallbackNode{executor, std::move(function)}
            , DoubleLinkedNode{&origin_node, origin_node.next_node}
            , fd_{-1}
            , events_{0}
        {
            origin_node.next_node->prev_node = this;
            origin_node.next_node            = this;
        }

        ~AwaitableNode() override
        {
            if (fd_ >= 0)
            {
                getExecutor().total_awaitables_--;
            }

            if ((nullptr != prev_node) && (nullptr != next_node))
            {
                prev_node->next_node = next_node;
                next_node->prev_node = prev_node;
            }
        }

        AwaitableNode(AwaitableNode&& other) noexcept
            : CallbackNode(std::move(other))
            , DoubleLinkedNode{std::exchange(other.prev_node, nullptr), std::exchange(other.next_node, nullptr)}
            , fd_{std::exchange(other.fd_, -1)}
            , events_{std::exchange(other.events_, 0)}
        {
            prev_node->next_node = this;
            next_node->prev_node = this;
        }

        AwaitableNode(const AwaitableNode&)                      = delete;
        AwaitableNode& operator=(const AwaitableNode&)           = delete;
        AwaitableNode& operator=(AwaitableNode&& other) noexcept = delete;

        int fd() const noexcept
        {
            return fd_;
        }

        PollEvents events() const noexcept
        {
            return events_;
        }

        void setup(const int fd, const PollEvents events) noexcept
        {
            CETL_DEBUG_ASSERT(fd >= 0, "");
            CETL_DEBUG_ASSERT(events != 0, "");

            fd_     = fd;
            events_ = events;

            getExecutor().total_awaitables_++;
        }

    private:
        Self& getExecutor() noexcept
        {
            return static_cast<Self&>(executor());
        }

        // MARK: Data members:

        int        fd_;
        PollEvents events_;

    };  // AwaitableNode

    // MARK: - Data members:

    using PollFds = cetl::VariableLengthArray<pollfd, cetl::pmr::polymorphic_allocator<pollfd>>;
    using CallbackInterfaces =
        cetl::VariableLengthArray<Callback::Interface*, cetl::pmr::polymorphic_allocator<Callback::Interface*>>;

    DoubleLinkedNode   awaitable_nodes_;
    std::size_t        total_awaitables_;
    PollFds            poll_fds_;
    CallbackInterfaces callback_interfaces_;

};  // PollSingleThreadedExecutor

}  // namespace posix
}  // namespace platform
}  // namespace example

#endif  // EXAMPLE_PLATFORM_POSIX_POLL_SINGLE_THREADED_EXECUTOR_HPP_INCLUDED
