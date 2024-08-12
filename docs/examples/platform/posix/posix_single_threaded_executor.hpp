/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef EXAMPLE_PLATFORM_POSIX_SINGLE_THREADED_EXECUTOR_HPP_INCLUDED
#define EXAMPLE_PLATFORM_POSIX_SINGLE_THREADED_EXECUTOR_HPP_INCLUDED

#include "posix_executor_extension.hpp"
#include "posix_platform_error.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/rtti.hpp>
#include <cetl/variable_length_array.hpp>
#include <libcyphal/common/cavl/cavl.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/platform/single_threaded_executor.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/types.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <errno.h>
#include <limits>
#include <sys/poll.h>
#include <thread>
#include <tuple>

namespace example
{
namespace platform
{
namespace posix
{

class PosixSingleThreadedExecutor final : public libcyphal::platform::SingleThreadedExecutor,
                                          public IPosixExecutorExtension
{
public:
    explicit PosixSingleThreadedExecutor(cetl::pmr::memory_resource& memory_resource)
        : total_awaitables_{0}
        , awaitable_nodes_allocator_{&memory_resource}
        , poll_fds_{&memory_resource}
        , callback_interfaces_{&memory_resource}
    {
    }

    PosixSingleThreadedExecutor(const PosixSingleThreadedExecutor&)                = delete;
    PosixSingleThreadedExecutor(PosixSingleThreadedExecutor&&) noexcept            = delete;
    PosixSingleThreadedExecutor& operator=(const PosixSingleThreadedExecutor&)     = delete;
    PosixSingleThreadedExecutor& operator=(PosixSingleThreadedExecutor&&) noexcept = delete;

    ~PosixSingleThreadedExecutor() override
    {
        // Just in case release whatever awaitable nodes left, but properly used `Callback::Interface`-s
        // (see `onCallbackHandling`) should have removed them all.
        //
        CETL_DEBUG_ASSERT(awaitable_nodes_.empty(), "");
        awaitable_nodes_.traversePostOrder([this](auto& node) { destroyAwaitableNode(node); });
    }

    using PollFailure = cetl::variant<libcyphal::transport::MemoryError,
                                      libcyphal::transport::PlatformError,
                                      libcyphal::transport::ArgumentError>;

    cetl::optional<PollFailure> pollAwaitableResourcesFor(const cetl::optional<libcyphal::Duration> timeout)
    {
        CETL_DEBUG_ASSERT(!awaitable_nodes_.empty() || timeout,
                          "Infinite timeout without awaitables means that we will sleep forever.");

        if (awaitable_nodes_.empty())
        {
            if (!timeout)
            {
                return libcyphal::transport::ArgumentError{};
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
        awaitable_nodes_.traverseInOrder([this](AwaitableNode& node) {
            //
            callback_interfaces_.push_back(node.cb_interface());
            poll_fds_.push_back({node.fd(), static_cast<short int>(node.pollEvents()), 0});
        });
        if ((total_awaitables_ != poll_fds_.size()) || (total_awaitables_ != callback_interfaces_.size()))
        {
            return libcyphal::transport::MemoryError{};
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
    void onCallbackHandling(const CallbackHandling::Variant& event_var) override
    {
        cetl::visit([this](const auto& event) { onCallbackHandlingImpl(event); }, event_var);
    }

    // MARK: - IPosixExecutorExtension

    bool scheduleCallbackWhen(Callback::Any& callback, const WhenCondition::Variant& when_condition) override
    {
        Callback::Interface* const cb_interface = callback.getInterface();

        return cetl::visit(
            [this, cb_interface](const auto& condition) {
                //
                return scheduleCallbackWhenImpl(cb_interface, condition);
            },
            when_condition);
    }

    // MARK: - RTTI

    CETL_NODISCARD void* _cast_(const cetl::type_id& id) & noexcept override
    {
        if (id == IPosixExecutorExtension::_get_type_id_())
        {
            return static_cast<IPosixExecutorExtension*>(this);
        }
        return base::_cast_(id);
    }
    CETL_NODISCARD const void* _cast_(const cetl::type_id& id) const& noexcept override
    {
        if (id == IPosixExecutorExtension::_get_type_id_())
        {
            return static_cast<const IPosixExecutorExtension*>(this);
        }
        return base::_cast_(id);
    }

private:
    using PollEvents = std::uint16_t;
    using base       = SingleThreadedExecutor;

    class AwaitableNode final : public cavl::Node<AwaitableNode>
    {
    public:
        using Node::getChildNode;

        explicit AwaitableNode(Callback::Interface* const cb_interface)
            : cb_interface_{cb_interface}
            , fd_{-1}
            , poll_events_{0}
        {
        }

        ~AwaitableNode() = default;

        AwaitableNode(const AwaitableNode&)                = delete;
        AwaitableNode(AwaitableNode&&) noexcept            = delete;
        AwaitableNode& operator=(const AwaitableNode&)     = delete;
        AwaitableNode& operator=(AwaitableNode&&) noexcept = delete;

        Callback::Interface*& cb_interface() noexcept
        {
            return cb_interface_;
        }

        int& fd() noexcept
        {
            return fd_;
        }

        PollEvents& pollEvents() noexcept
        {
            return poll_events_;
        }

        CETL_NODISCARD std::int8_t compareByInterface(Callback::Interface* const cb_interface) const noexcept
        {
            if (cb_interface == cb_interface_)
            {
                return 0;
            }
            return (cb_interface > cb_interface_) ? +1 : -1;
        }

    private:
        // MARK: Data members:

        Callback::Interface* cb_interface_;
        int                  fd_;
        PollEvents           poll_events_;

    };  // AwaitableNode

    void onCallbackHandlingImpl(const CallbackHandling::Moved& moved) noexcept
    {
        if (auto* const awaitable_node = awaitable_nodes_.search(  //
                [&moved](const AwaitableNode& node) {              // predicate
                    //
                    return node.compareByInterface(moved.old_interface);
                }))
        {
            awaitable_nodes_.remove(awaitable_node);
            awaitable_node->cb_interface() = moved.new_interface;

            awaitable_nodes_.search(                   //
                [&moved](const AwaitableNode& node) {  // predicate
                    //
                    return node.compareByInterface(moved.new_interface);
                },
                [awaitable_node] { return awaitable_node; });
        }
    }

    void onCallbackHandlingImpl(const CallbackHandling::Removed& removed) noexcept
    {
        if (auto* const awaitable_node = awaitable_nodes_.search(  //
                [&removed](const AwaitableNode& node) {            // predicate
                    //
                    return node.compareByInterface(removed.old_interface);
                }))
        {
            awaitable_nodes_.remove(awaitable_node);
            destroyAwaitableNode(*awaitable_node);
        }
    }

    CETL_NODISCARD AwaitableNode* ensureAwaitableNode(Callback::Interface* const cb_interface)
    {
        const std::tuple<AwaitableNode*, bool> node_existing = awaitable_nodes_.search(  //
            [cb_interface](const AwaitableNode& node) {                                  // predicate
                //
                return node.compareByInterface(cb_interface);
            },
            [this, cb_interface] {  // factory
                //
                return makeAwaitableNode(cb_interface);
            });

        return std::get<0>(node_existing);
    }

    CETL_NODISCARD AwaitableNode* makeAwaitableNode(Callback::Interface* const cb_interface)
    {
        // Stop allocations if we reach the maximum number of awaitables supported by `poll`.
        CETL_DEBUG_ASSERT(total_awaitables_ < std::numeric_limits<int>::max(), "");
        if (total_awaitables_ >= std::numeric_limits<int>::max())
        {
            return nullptr;
        }

        AwaitableNode* const node = awaitable_nodes_allocator_.allocate(1);
        if (nullptr != node)
        {
            awaitable_nodes_allocator_.construct(node, cb_interface);
        }

        ++total_awaitables_;
        return node;
    }

    void destroyAwaitableNode(AwaitableNode& awaitable_node)
    {
        // No Sonar cpp:M23_329 b/c we do our own low-level PMR management here.
        awaitable_node.~AwaitableNode();  // NOSONAR cpp:M23_329
        awaitable_nodes_allocator_.deallocate(&awaitable_node, 1);

        --total_awaitables_;
    }

    bool scheduleCallbackWhenImpl(Callback::Interface* cb_interface, const WhenCondition::HandleReadable& readable)
    {
        auto* const awaitable_node = ensureAwaitableNode(cb_interface);
        if (nullptr == awaitable_node)
        {
            return false;
        }

        awaitable_node->fd() = readable.fd;
        awaitable_node->pollEvents() |= static_cast<PollEvents>(POLLIN);

        return true;
    }

    bool scheduleCallbackWhenImpl(Callback::Interface* cb_interface, const WhenCondition::HandleWritable& writable)
    {
        auto* const awaitable_node = ensureAwaitableNode(cb_interface);
        if (nullptr == awaitable_node)
        {
            return false;
        }

        awaitable_node->fd() = writable.fd;
        awaitable_node->pollEvents() |= static_cast<PollEvents>(POLLOUT);

        return true;
    }

    // MARK: - Data members:

    using PollFds = cetl::VariableLengthArray<pollfd, cetl::pmr::polymorphic_allocator<pollfd>>;
    using CallbackInterfaces =
        cetl::VariableLengthArray<Callback::Interface*, cetl::pmr::polymorphic_allocator<Callback::Interface*>>;

    std::size_t                                    total_awaitables_;
    cavl::Tree<AwaitableNode>                      awaitable_nodes_;
    libcyphal::detail::PmrAllocator<AwaitableNode> awaitable_nodes_allocator_;
    PollFds                                        poll_fds_;
    CallbackInterfaces                             callback_interfaces_;

};  // PosixSingleThreadedExecutor

}  // namespace posix
}  // namespace platform
}  // namespace example

#endif  // EXAMPLE_PLATFORM_POSIX_SINGLE_THREADED_EXECUTOR_HPP_INCLUDED
