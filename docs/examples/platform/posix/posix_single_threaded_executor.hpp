/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef EXAMPLE_PLATFORM_POSIX_SINGLE_THREADED_EXECUTOR_HPP
#define EXAMPLE_PLATFORM_POSIX_SINGLE_THREADED_EXECUTOR_HPP

#include "posix_executor_extension.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/rtti.hpp>
#include <cetl/variable_length_array.hpp>
#include <libcyphal/common/cavl/cavl.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/platform/single_threaded_executor.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sys/poll.h>
#include <tuple>

namespace example
{
namespace platform
{
namespace posix
{

class PosixSingleThreadedExecutor : public libcyphal::platform::SingleThreadedExecutor, public IPosixExecutorExtension
{
public:
    explicit PosixSingleThreadedExecutor(cetl::pmr::memory_resource& memory_resource)
        : base{memory_resource}
        , awaitable_nodes_allocator_{&memory_resource}
        , poll_fds_{&memory_resource}
        , callback_ids_{&memory_resource}
    {
    }

    void pollAwaitables(const libcyphal::Duration timeout)
    {
        if (awaitable_nodes_.empty())
        {
            return;
        }

        callback_ids_.clear();
        poll_fds_.clear();
        awaitable_nodes_.traverse([this](AwaitableNode& node) {
            //
            CETL_DEBUG_ASSERT(node.fd() >= 0, "");
            CETL_DEBUG_ASSERT(node.pollEvents() != 0, "");

            callback_ids_.push_back(node.callbackId());
            poll_fds_.push_back({node.fd(), static_cast<short int>(node.pollEvents()), 0});
        });

        // Make sure that timeout is within the range of `::poll()`'s `int` timeout parameter.
        // Any possible negative timeout will be treated as zero (return immediately from the poll).
        //
        using PollDuration = std::chrono::milliseconds;
        const auto clamped_timeout_ms =
            static_cast<int>(std::max(static_cast<PollDuration::rep>(0),
                                      std::min(std::chrono::duration_cast<PollDuration>(timeout).count(),
                                               static_cast<PollDuration::rep>(std::numeric_limits<int>::max()))));

        int poll_result = ::poll(poll_fds_.data(), poll_fds_.size(), clamped_timeout_ms);
        if (poll_result <= 0)
        {
            return;
        }

        const auto now_time = now();
        for (std::size_t index = 0; (index < poll_fds_.size()) && (poll_result > 0); ++index)
        {
            const pollfd& poll_fd = poll_fds_[index];
            if (poll_fd.revents != 0)
            {
                --poll_result;
                if (0 != (static_cast<PollEvents>(poll_fd.revents) & static_cast<PollEvents>(poll_fd.events)))
                {
                    const Callback::Id callback_id = callback_ids_[index];
                    scheduleCallbackByIdAt(callback_id, now_time);
                }
            }
        }
    }

protected:
    void didRemoveCallback(const Callback::Id callback_id) override
    {
        auto* const awaitable_node = awaitable_nodes_.search(  //
            [callback_id](const AwaitableNode& node) {         // predicate
                return node.compareByCallbackId(callback_id);
            });
        if (nullptr == awaitable_node)
        {
            return;
        }

        awaitable_nodes_.remove(awaitable_node);
        destroyWaitableNode(awaitable_node);
    }

    // MARK: - IPosixExecutorExtension

    bool scheduleCallbackWhen(const Callback::Id callback_id, const WhenCondition::Variant& condition) override
    {
        return cetl::visit(
            [this, callback_id](const auto& condition) {  //
                return scheduleCallbackWhenImpl(callback_id, condition);
            },
            condition);
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
    using base       = libcyphal::platform::SingleThreadedExecutor;

    class AwaitableNode final : public cavl::Node<AwaitableNode>
    {
    public:
        explicit AwaitableNode(const Callback::Id callback_id)
            : callback_id_{callback_id}
            , fd_{-1}
            , poll_events_{0}
        {
        }

        ~AwaitableNode() = default;

        AwaitableNode(const AwaitableNode&)                = delete;
        AwaitableNode(AwaitableNode&&) noexcept            = delete;
        AwaitableNode& operator=(const AwaitableNode&)     = delete;
        AwaitableNode& operator=(AwaitableNode&&) noexcept = delete;

        Callback::Id callbackId() const noexcept
        {
            return callback_id_;
        }

        int& fd() noexcept
        {
            return fd_;
        }

        PollEvents& pollEvents() noexcept
        {
            return poll_events_;
        }

        CETL_NODISCARD std::int8_t compareByCallbackId(const Callback::Id callback_id) const noexcept
        {
            if (callback_id == callback_id_)
            {
                return 0;
            }
            return (callback_id > callback_id_) ? +1 : -1;
        }

    private:
        // MARK: Data members:

        const Callback::Id callback_id_;
        int                fd_;
        PollEvents         poll_events_;

    };  // AwaitableNode

    CETL_NODISCARD AwaitableNode* ensureWaitableNode(const Callback::Id callback_id)
    {
        const std::tuple<AwaitableNode*, bool> node_existing = awaitable_nodes_.search(  //
            [callback_id](const AwaitableNode& node) {                                   // predicate
                return node.compareByCallbackId(callback_id);
            },
            [this, callback_id]() {  // factory
                return makeWaitableNode(callback_id);
            });

        return std::get<0>(node_existing);
    }

    CETL_NODISCARD AwaitableNode* makeWaitableNode(const Callback::Id callback_id)
    {
        AwaitableNode* const node = awaitable_nodes_allocator_.allocate(1);
        if (nullptr != node)
        {
            awaitable_nodes_allocator_.construct(node, callback_id);
        }
        return node;
    }

    void destroyWaitableNode(AwaitableNode* const awaitable_node)
    {
        CETL_DEBUG_ASSERT(nullptr != awaitable_node, "");

        // No Sonar cpp:M23_329 b/c we do our own low-level PMR management here.
        awaitable_node->~AwaitableNode();  // NOSONAR cpp:M23_329
        awaitable_nodes_allocator_.deallocate(awaitable_node, 1);
    }

    bool scheduleCallbackWhenImpl(const Callback::Id callback_id, const WhenCondition::HandleReadable& readable)
    {
        CETL_DEBUG_ASSERT(readable.fd >= 0, "");

        auto* const awaitable_node = ensureWaitableNode(callback_id);
        if (nullptr == awaitable_node)
        {
            return false;
        }

        awaitable_node->fd() = readable.fd;
        awaitable_node->pollEvents() |= static_cast<PollEvents>(POLLIN);

        return true;
    }

    bool scheduleCallbackWhenImpl(const Callback::Id callback_id, const WhenCondition::HandleWritable& writable)
    {
        CETL_DEBUG_ASSERT(writable.fd >= 0, "");

        auto* const awaitable_node = ensureWaitableNode(callback_id);
        if (nullptr == awaitable_node)
        {
            return false;
        }

        awaitable_node->fd() = writable.fd;
        awaitable_node->pollEvents() |= static_cast<PollEvents>(POLLOUT);

        return true;
    }

    // MARK: - Data members:

    using PollFds     = cetl::VariableLengthArray<pollfd, cetl::pmr::polymorphic_allocator<pollfd>>;
    using CallbackIds = cetl::VariableLengthArray<Callback::Id, cetl::pmr::polymorphic_allocator<Callback::Id>>;

    cavl::Tree<AwaitableNode>                      awaitable_nodes_;
    libcyphal::detail::PmrAllocator<AwaitableNode> awaitable_nodes_allocator_;
    PollFds                                        poll_fds_;
    CallbackIds                                    callback_ids_;

};  // PosixSingleThreadedExecutor

}  // namespace posix
}  // namespace platform
}  // namespace example

#endif  // EXAMPLE_PLATFORM_POSIX_SINGLE_THREADED_EXECUTOR_HPP
