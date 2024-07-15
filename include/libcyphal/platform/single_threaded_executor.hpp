/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PLATFORM_SINGLE_THREADED_EXECUTOR_HPP
#define LIBCYPHAL_PLATFORM_SINGLE_THREADED_EXECUTOR_HPP

#include "libcyphal/common/cavl/cavl.hpp"
#include "libcyphal/executor.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

#include <chrono>
#include <tuple>
#include <utility>

namespace libcyphal
{
namespace platform
{

class SingleThreadedExecutor : public IExecutor
{
public:
    explicit SingleThreadedExecutor(cetl::pmr::memory_resource& memory_resource)
        : allocator_{&memory_resource}
    {
    }

    virtual ~SingleThreadedExecutor() = default;

    SingleThreadedExecutor(const SingleThreadedExecutor&)                = delete;
    SingleThreadedExecutor(SingleThreadedExecutor&&) noexcept            = delete;
    SingleThreadedExecutor& operator=(const SingleThreadedExecutor&)     = delete;
    SingleThreadedExecutor& operator=(SingleThreadedExecutor&&) noexcept = delete;

    // MARK: - IExecutor

    TimePoint now() const noexcept override
    {
        const auto duration = std::chrono::steady_clock::now().time_since_epoch();
        return TimePoint{} + std::chrono::duration_cast<Duration>(duration);
    }

    using IExecutor::registerCallback;

protected:
    CETL_NODISCARD cetl::optional<Callback::Id> appendCallback(const bool           is_auto_remove,
                                                               Callback::Function&& function) override
    {
        CETL_DEBUG_ASSERT(function, "");

        auto* new_callback_node = makeCallbackNode(std::move(function), is_auto_remove);
        if (nullptr == new_callback_node)
        {
            return cetl::nullopt;
        }

        const auto new_callback_id      = ++last_callback_id_;
        new_callback_node->callbackId() = new_callback_id;

        const std::tuple<CallbackNode*, bool> reg_node_existing = registered_nodes_.search(  //
            [new_callback_id](const CallbackNode& node) {                                    // predicate
                return node.compareByCallbackId(new_callback_id);
            },
            [new_callback_node]() { return new_callback_node; });  // factory

        (void) reg_node_existing;
        CETL_DEBUG_ASSERT(!std::get<1>(reg_node_existing), "Callback id collision detected.");
        CETL_DEBUG_ASSERT(new_callback_node == std::get<0>(reg_node_existing), "Unexpected not the new node.");

        return cetl::optional<Callback::Id>{new_callback_id};
    }

    bool scheduleCallbackByIdAt(const Callback::Id callback_id, const TimePoint time_point) override
    {
        auto* const callback_node = registered_nodes_.search(  //
            [callback_id](const CallbackNode& node) {          // predicate
                return node.compareByCallbackId(callback_id);
            });
        if (nullptr == callback_node)
        {
            return false;
        }

        // Remove previous scheduling node (if any),
        // and then re/insert the node with updated/given execution time.
        //
        scheduled_nodes_.remove(callback_node);
        callback_node->executionTime() = time_point;
        //
        const std::tuple<SchedulingNode*, bool> sched_node_existing = scheduled_nodes_.search(  //
            [time_point](const SchedulingNode& scheduling_node) {                               // predicate
                return scheduling_node.compareByExecutionTime(time_point);
            },
            [callback_node]() { return callback_node; });  // factory

        (void) sched_node_existing;
        CETL_DEBUG_ASSERT(!std::get<1>(sched_node_existing), "Unexpected existing scheduling node.");
        CETL_DEBUG_ASSERT(callback_node == std::get<0>(sched_node_existing), "Unexpected callback node.");

        return true;
    }

    void removeCallbackById(const Callback::Id callback_id) override
    {
        auto* const callback_node = registered_nodes_.search(  //
            [callback_id](const CallbackNode& node) {          // predicate
                return node.compareByCallbackId(callback_id);
            });
        if (nullptr == callback_node)
        {
            return;
        }

        if (callback_node->isScheduled())
        {
            scheduled_nodes_.remove(callback_node);
        }

        registered_nodes_.remove(callback_node);
        destroyCallbackNode(callback_node);
    }

private:
    class SchedulingNode : public cavl::Node<SchedulingNode>
    {
    public:
        SchedulingNode(const SchedulingNode&)                = delete;
        SchedulingNode(SchedulingNode&&) noexcept            = delete;
        SchedulingNode& operator=(const SchedulingNode&)     = delete;
        SchedulingNode& operator=(SchedulingNode&&) noexcept = delete;

        bool isAutoRemove() const noexcept
        {
            return is_auto_remove_;
        }

        TimePoint& executionTime() noexcept
        {
            return execution_time_;
        }

        void operator()(const TimePoint time_point) const
        {
            function_(time_point);
        }

        CETL_NODISCARD int compareByExecutionTime(const TimePoint execution_time) const noexcept
        {
            return (execution_time_ <= execution_time) ? -1 : 1;
        }

    protected:
        SchedulingNode(Callback::Function&& function, const bool is_auto_remove)
            : function_{std::move(function)}
            , is_auto_remove_{is_auto_remove}
        {
        }
        ~SchedulingNode() = default;

    private:
        // MARK: Data members:

        const Callback::Function function_;
        const bool               is_auto_remove_;
        TimePoint                execution_time_;

    };  // SchedulingNode

    class CallbackNode final : public cavl::Node<CallbackNode>, public SchedulingNode
    {
    public:
        using CallbackId = IExecutor::Callback::Id;

        CallbackNode(Callback::Function&& function, const bool is_auto_remove)
            : SchedulingNode{std::move(function), is_auto_remove}
            , callback_id_{0}
            , is_scheduled_{false}
        {
        }
        ~CallbackNode() = default;

        CallbackNode(const CallbackNode&)                = delete;
        CallbackNode(CallbackNode&&) noexcept            = delete;
        CallbackNode& operator=(const CallbackNode&)     = delete;
        CallbackNode& operator=(CallbackNode&&) noexcept = delete;

        CallbackId& callbackId() noexcept
        {
            return callback_id_;
        }

        bool& isScheduled() noexcept
        {
            return is_scheduled_;
        }

        CETL_NODISCARD int compareByCallbackId(const CallbackId callback_id) const noexcept
        {
            if (callback_id_ == callback_id)
            {
                return 0;
            }
            return (callback_id_ < callback_id) ? -1 : 1;
        }

    private:
        // MARK: Data members:

        CallbackId callback_id_;
        bool       is_scheduled_;

    };  // CallbackNode

    CETL_NODISCARD CallbackNode* makeCallbackNode(Callback::Function&& function, const bool is_auto_remove)
    {
        CallbackNode* const node = allocator_.allocate(1);
        if (nullptr != node)
        {
            allocator_.construct(node, std::move(function), is_auto_remove);
        }
        return node;
    }

    void destroyCallbackNode(CallbackNode* callback_node)
    {
        CETL_DEBUG_ASSERT(nullptr != callback_node, "");

        // No Sonar cpp:M23_329 b/c we do our own low-level PMR management here.
        callback_node->~CallbackNode();  // NOSONAR cpp:M23_329
        allocator_.deallocate(callback_node, 1);
    }

    // MARK: - Data members:

    libcyphal::detail::PmrAllocator<CallbackNode> allocator_;
    cavl::Tree<SchedulingNode>                    scheduled_nodes_;
    cavl::Tree<CallbackNode>                      registered_nodes_;
    Callback::Id                                  last_callback_id_{0};

};  // SingleThreadedExecutor

}  // namespace platform
}  // namespace libcyphal

#endif  // LIBCYPHAL_PLATFORM_SINGLE_THREADED_EXECUTOR_HPP
