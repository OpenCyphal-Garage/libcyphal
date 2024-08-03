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
        , callback_handles_{&memory_resource}
    {
    }

    PosixSingleThreadedExecutor(const PosixSingleThreadedExecutor&)                = delete;
    PosixSingleThreadedExecutor(PosixSingleThreadedExecutor&&) noexcept            = delete;
    PosixSingleThreadedExecutor& operator=(const PosixSingleThreadedExecutor&)     = delete;
    PosixSingleThreadedExecutor& operator=(PosixSingleThreadedExecutor&&) noexcept = delete;

    ~PosixSingleThreadedExecutor() override
    {
        // Just in case release whatever awaitable nodes left, but properly used `Callback::Handle`-s
        // (aka "handle must not outlive executor") should have removed them all.
        //
        CETL_DEBUG_ASSERT(awaitable_nodes_.empty(), "");
        awaitable_nodes_.postOrderTraverse([this](auto& node) { destroyAwaitableNode(node); });
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
        callback_handles_.clear();
        awaitable_nodes_.traverse([this](AwaitableNode& node) {
            //
            CETL_DEBUG_ASSERT(node.fd() >= 0, "");
            CETL_DEBUG_ASSERT(node.pollEvents() != 0, "");

            callback_handles_.push_back(node.handle());
            poll_fds_.push_back({node.fd(), static_cast<short int>(node.pollEvents()), 0});
        });
        if ((total_awaitables_ != poll_fds_.size()) || (total_awaitables_ != callback_handles_.size()))
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
                    const Callback::Handle cb_handle = callback_handles_[index];
                    scheduleCallbackByHandle(cb_handle, Callback::Schedule::Once{now_time});
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

        callback_handles_.clear();
        callback_handles_.shrink_to_fit();
    }

protected:
    void onCallbackHandling(const Callback::Handle old_handle, const Callback::Handle new_handle) noexcept override
    {
        auto* const awaitable_node = awaitable_nodes_.search(  //
            [old_handle](const AwaitableNode& node) {          // predicate
                return node.compareByHandle(old_handle);
            });
        if (nullptr == awaitable_node)
        {
            return;
        }

        awaitable_nodes_.remove(awaitable_node);

        if (new_handle)
        {
            awaitable_node->handle() = new_handle;
            awaitable_nodes_.search(  //
                [new_handle](const AwaitableNode& node) { return node.compareByHandle(new_handle); },
                [awaitable_node]() { return awaitable_node; });
        }
        else
        {
            destroyAwaitableNode(awaitable_node);
        }
    }

    // MARK: - IPosixExecutorExtension

    bool scheduleCallbackWhen(Callback::Any& callback, const WhenCondition::Variant& when_condition) override
    {
        const Callback::Handle cb_handle = callbackToHandle(callback);

        return cetl::visit(
            [this, cb_handle](const auto& condition) {  //
                return scheduleCallbackWhenImpl(cb_handle, condition);
            },
            when_condition);
    }

    // MARK: - cetl::rtti

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

        explicit AwaitableNode(const Callback::Handle cb_handle)
            : cb_handle_{cb_handle}
            , fd_{-1}
            , poll_events_{0}
        {
        }

        ~AwaitableNode() = default;

        AwaitableNode(const AwaitableNode&)                = delete;
        AwaitableNode(AwaitableNode&&) noexcept            = delete;
        AwaitableNode& operator=(const AwaitableNode&)     = delete;
        AwaitableNode& operator=(AwaitableNode&&) noexcept = delete;

        Callback::Handle& handle() noexcept
        {
            return cb_handle_;
        }

        int& fd() noexcept
        {
            return fd_;
        }

        PollEvents& pollEvents() noexcept
        {
            return poll_events_;
        }

        CETL_NODISCARD std::int8_t compareByHandle(const Callback::Handle cb_handle) const noexcept
        {
            if (cb_handle == cb_handle_)
            {
                return 0;
            }
            return (cb_handle > cb_handle_) ? +1 : -1;
        }

    private:
        // MARK: Data members:

        Callback::Handle cb_handle_;
        int              fd_;
        PollEvents       poll_events_;

    };  // AwaitableNode

    CETL_NODISCARD AwaitableNode* ensureAwaitableNode(const Callback::Handle cb_handle)
    {
        const std::tuple<AwaitableNode*, bool> node_existing = awaitable_nodes_.search(  //
            [cb_handle](const AwaitableNode& node) {                                     // predicate
                return node.compareByHandle(cb_handle);
            },
            [this, cb_handle]() {  // factory
                return makeAwaitableNode(cb_handle);
            });

        return std::get<0>(node_existing);
    }

    CETL_NODISCARD AwaitableNode* makeAwaitableNode(const Callback::Handle cb_handle)
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
            awaitable_nodes_allocator_.construct(node, cb_handle);
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

    bool scheduleCallbackWhenImpl(const Callback::Handle cb_handle, const WhenCondition::HandleReadable& readable)
    {
        CETL_DEBUG_ASSERT(readable.fd >= 0, "");

        auto* const awaitable_node = ensureAwaitableNode(cb_handle);
        if (nullptr == awaitable_node)
        {
            return false;
        }

        awaitable_node->fd() = readable.fd;
        awaitable_node->pollEvents() |= static_cast<PollEvents>(POLLIN);

        return true;
    }

    bool scheduleCallbackWhenImpl(const Callback::Handle cb_handle, const WhenCondition::HandleWritable& writable)
    {
        CETL_DEBUG_ASSERT(writable.fd >= 0, "");

        auto* const awaitable_node = ensureAwaitableNode(cb_handle);
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
    using CallbackHandles =
        cetl::VariableLengthArray<Callback::Handle, cetl::pmr::polymorphic_allocator<Callback::Handle>>;

    std::size_t                                    total_awaitables_;
    cavl::Tree<AwaitableNode>                      awaitable_nodes_;
    libcyphal::detail::PmrAllocator<AwaitableNode> awaitable_nodes_allocator_;
    PollFds                                        poll_fds_;
    CallbackHandles                                callback_handles_;

};  // PosixSingleThreadedExecutor

}  // namespace posix
}  // namespace platform
}  // namespace example

#endif  // EXAMPLE_PLATFORM_POSIX_SINGLE_THREADED_EXECUTOR_HPP_INCLUDED
