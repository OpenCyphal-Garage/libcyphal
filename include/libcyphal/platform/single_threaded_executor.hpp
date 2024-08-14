/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PLATFORM_SINGLE_THREADED_EXECUTOR_HPP_INCLUDED
#define LIBCYPHAL_PLATFORM_SINGLE_THREADED_EXECUTOR_HPP_INCLUDED

#include "libcyphal/common/cavl/cavl.hpp"
#include "libcyphal/executor.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/rtti.hpp>
#include <cetl/unbounded_variant.hpp>
#include <cetl/visit_helpers.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <tuple>
#include <utility>

namespace libcyphal
{
namespace platform
{

/// @brief Defines platform agnostic single-threaded executor.
///
class SingleThreadedExecutor : public virtual IExecutor
{
public:
    SingleThreadedExecutor() {};
    virtual ~SingleThreadedExecutor() = default;

    SingleThreadedExecutor(const SingleThreadedExecutor&)                = delete;
    SingleThreadedExecutor(SingleThreadedExecutor&&) noexcept            = delete;
    SingleThreadedExecutor& operator=(const SingleThreadedExecutor&)     = delete;
    SingleThreadedExecutor& operator=(SingleThreadedExecutor&&) noexcept = delete;

    struct SpinResult
    {
        /// Time of the next scheduled callback to execute,
        /// or `cetl::nullopt` if there are no scheduled callbacks.
        /// This can be used to let the application sleep/poll when there are no callbacks pending.
        cetl::optional<TimePoint> next_exec_time;

        /// An approximation of the maximum lateness observed during the spin call
        /// (the real slack may be worse than the approximation).
        /// This is always non-negative.
        Duration worst_lateness;

        /// An approximation of the current time such that (approx_now <= Clock::now()).
        /// This is helpful because the time may be expensive to sample.
        TimePoint approx_now;
    };

    CETL_NODISCARD SpinResult spinOnce()
    {
        if (callback_nodes_.empty())
        {
            return {cetl::nullopt, {}, now()};
        }

        SpinResult spin_result{{}, {}, TimePoint::min()};
        while (auto* const callback_node_ptr = callback_nodes_.min())
        {
            auto& callback_node = *callback_node_ptr;

            if (const auto exec_time = isNowTimeToExecute(callback_node.nextExecTime(), spin_result))
            {
                const Callback::Arg arg{*exec_time, spin_result.approx_now};

                adjustNextExecTimeOf(callback_node, [&arg](auto& cb_node) {
                    //
                    cb_node.reschedule(arg);
                });

                callback_node(arg);
            }
            else
            {
                // Nodes are sorted by execution time, so we can stop here b/c the rest of the nodes
                // will have execution times later than the current node execution time.
                break;
            }

        }  // while there is a pending callback to execute

        CETL_DEBUG_ASSERT(spin_result.approx_now > TimePoint::min(), "");
        CETL_DEBUG_ASSERT(spin_result.worst_lateness >= Duration::zero(), "");

        return spin_result;
    }

    // MARK: - IExecutor

    TimePoint now() const noexcept override
    {
        const auto duration = std::chrono::steady_clock::now().time_since_epoch();
        return TimePoint{} + std::chrono::duration_cast<Duration>(duration);
    }

    CETL_NODISCARD Callback::Any registerCallback(Callback::Function&& function) override
    {
        CallbackNode new_cb_node{*this, std::move(function)};
        insertCallbackNode(new_cb_node);
        return {std::move(new_cb_node)};
    }

protected:
    /// No Sonar cpp:S4963 b/c `CallbackNode` supports move operation.
    ///
    class CallbackNode : public cavl::Node<CallbackNode>, public Callback::Interface  // NOSONAR cpp:S4963
    {
    public:
        CallbackNode(SingleThreadedExecutor& executor, Callback::Function&& function)
            : executor_{executor}
            , function_{std::move(function)}
            , next_exec_time_{TimePointNever()}
            , schedule_{cetl::nullopt}
        {
            CETL_DEBUG_ASSERT(function_, "");
        }
        virtual ~CallbackNode()
        {
            if (isLinked())
            {
                executor_.callback_nodes_.remove(this);
            }
        };

        CallbackNode(CallbackNode&& other) noexcept
            : Node(std::move(static_cast<Node&>(other)))
            , executor_{other.executor_}
            , function_{std::move(other.function_)}
            , next_exec_time_{other.next_exec_time_}
            , schedule_{other.schedule_}
        {
        }

        CallbackNode(const CallbackNode&)                      = delete;
        CallbackNode& operator=(const CallbackNode&)           = delete;
        CallbackNode& operator=(CallbackNode&& other) noexcept = delete;

        void reschedule(const Callback::Arg& arg)
        {
            CETL_DEBUG_ASSERT(schedule_, "");

            next_exec_time_ = cetl::visit(  //
                cetl::make_overloaded(      //
                    [](const Callback::Schedule::Once&) { return TimePointNever(); },
                    [&arg](const Callback::Schedule::Repeat& repeat) { return arg.exec_time + repeat.period; }),
                *schedule_);  // NOLINT(bugprone-unchecked-optional-access)
        }

        TimePoint nextExecTime() const noexcept
        {
            return next_exec_time_;
        }

        void operator()(const Callback::Arg& arg) const
        {
            function_(arg);
        }

        CETL_NODISCARD std::int8_t compareByExecutionTime(const TimePoint exec_time) const noexcept
        {
            // No two execution times compare equal, which allows us to have multiple nodes
            // with the same execution time in the tree. With two nodes sharing the same execution time,
            // the one added later is considered to be later.
            return (exec_time >= next_exec_time_) ? +1 : -1;
        }

        // MARK: Callback::Interface

        void schedule(const Callback::Schedule::Variant& schedule) override
        {
            CETL_DEBUG_ASSERT(isLinked(), "");

            schedule_ = schedule;
            executor_.adjustNextExecTimeOf(*this, [this, &schedule](auto&) {
                //
                next_exec_time_ = cetl::visit(  //
                    cetl::make_overloaded(      //
                        [](const Callback::Schedule::Once& once) { return once.exec_time; },
                        [](const Callback::Schedule::Repeat& repeat) { return repeat.exec_time; }),
                    schedule);
            });
        }

    protected:
        SingleThreadedExecutor& executor() noexcept
        {
            return executor_;
        }

    private:
        // MARK: Data members:

        SingleThreadedExecutor&                     executor_;
        Callback::Function                          function_;
        TimePoint                                   next_exec_time_;
        cetl::optional<Callback::Schedule::Variant> schedule_;

    };  // CallbackNode

    void insertCallbackNode(CallbackNode& callback_node)
    {
        const auto next_exec_time = callback_node.nextExecTime();

        const std::tuple<CallbackNode*, bool> cb_node_existing = callback_nodes_.search(  //
            [next_exec_time](const CallbackNode& other_node) {                            // predicate
                return other_node.compareByExecutionTime(next_exec_time);
            },
            [&callback_node]() { return &callback_node; });  // factory

        (void) cb_node_existing;
        CETL_DEBUG_ASSERT(!std::get<1>(cb_node_existing), "Unexpected existing callback node.");
        CETL_DEBUG_ASSERT(&callback_node == std::get<0>(cb_node_existing), "Unexpected callback node.");
    }

private:
    /// We use distant future as "never" time point.
    static constexpr TimePoint TimePointNever()
    {
        return TimePoint::max();
    }

    CETL_NODISCARD cetl::optional<TimePoint> isNowTimeToExecute(const TimePoint next_exec_time,
                                                                SpinResult&     inout_spin_result) const
    {
        if (inout_spin_result.approx_now < next_exec_time)
        {
            inout_spin_result.approx_now = now();
            if (inout_spin_result.approx_now < next_exec_time)
            {
                // To simplify node sorting `max` is used like "never" (aka "infinity") but result of spin
                // encodes this more explicitly as `cetl::nullopt` (aka "no next exec time").
                if (next_exec_time != TimePointNever())
                {
                    inout_spin_result.next_exec_time = next_exec_time;
                }

                return cetl::nullopt;
            }
        }

        inout_spin_result.worst_lateness = std::max(  //
            inout_spin_result.worst_lateness,
            inout_spin_result.approx_now - next_exec_time);

        return cetl::optional<TimePoint>{next_exec_time};
    }

    void removeCallbackNode(CallbackNode& callback_node)
    {
        callback_nodes_.remove(&callback_node);
    }

    template <typename AdjustAction>
    void adjustNextExecTimeOf(CallbackNode& callback_node, AdjustAction&& adjust_action)
    {
        // Removal and immediate insertion of the same node has sense b/c it will be (most probably) inserted
        // back at the different `callback_nodes_` tree position - according to the next exec time of the node.
        removeCallbackNode(callback_node);
        std::forward<AdjustAction>(adjust_action)(callback_node);
        insertCallbackNode(callback_node);
    }

    // MARK: - Data members:

    /// Holds AVL tree of registered callback node, sorted by the next execution time.
    cavl::Tree<CallbackNode> callback_nodes_;

};  // SingleThreadedExecutor

}  // namespace platform
}  // namespace libcyphal

#endif  // LIBCYPHAL_PLATFORM_SINGLE_THREADED_EXECUTOR_HPP_INCLUDED
