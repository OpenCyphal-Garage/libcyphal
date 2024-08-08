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
class SingleThreadedExecutor : public IExecutor
{
public:
    SingleThreadedExecutor()           = default;
    ~SingleThreadedExecutor() override = default;

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
    };

    CETL_NODISCARD SpinResult spinOnce()
    {
        SpinResult spin_result{};

        auto approx_now = TimePoint::min();

        while (auto* const callback_node_ptr = callback_nodes_.min())
        {
            auto& callback_node = *callback_node_ptr;

            const auto exec_time = callback_node.nextExecTime();
            if (approx_now < exec_time)
            {
                approx_now = now();
                if (approx_now < exec_time)
                {
                    if (exec_time != TimePoint::max())
                    {
                        spin_result.next_exec_time = exec_time;
                    }
                    break;
                }
            }

            spin_result.worst_lateness = std::max(approx_now - exec_time, spin_result.worst_lateness);

            removeCallbackNode(callback_node);
            callback_node.reschedule(exec_time);
            insertCallbackNode(callback_node);

            callback_node(approx_now);

        }  // while there is pending callback to execute

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
        CETL_DEBUG_ASSERT(function, "");

        CallbackNode new_callback_node{*this, std::move(function)};

        insertCallbackNode(new_callback_node);

        return {std::move(new_callback_node)};
    }

    void scheduleCallback(Callback::Any& callback, const Callback::Schedule::Variant& schedule) override
    {
        if (auto* const cb_node = callbackToNode(callback))
        {
            CETL_DEBUG_ASSERT(this == &cb_node->executor(), "");
            scheduleCallbackNode(*cb_node, schedule);
        }
    }

protected:
    static Callback::Handle callbackToHandle(Callback::Any& callback) noexcept
    {
        return static_cast<Callback::Handle>(*callbackToNode(callback));
    }

    void scheduleCallbackByHandle(const Callback::Handle cb_handle, const Callback::Schedule::Variant& schedule)
    {
        auto* const cb_node_ptr = CallbackNode::fromHandle(cb_handle);
        scheduleCallbackNode(*cb_node_ptr, schedule);
    }

    /// @brief Extension point for subclasses to observe callback lifetime.
    ///
    /// Called on callback node appending, movement and removal.
    ///
    /// @param old_handle Handle of the callback node before callback change.
    /// @param new_handle Handle of the callback node after callback change.
    ///
    /// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    virtual void onCallbackHandling(const Callback::Handle old_handle, const Callback::Handle new_handle) noexcept
    {
        // Nothing to do here b/c AVL node movement already updated its links,
        // but subclasses may override this method to update their own references to callbacks.
        (void) old_handle;
        (void) new_handle;
    }

private:
    // 49E40F04-42DC-481D-981C-46775698EED2
    using CallbackNodeTypeIdType = cetl::
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
        type_id_type<0x49, 0xE4, 0x0F, 0x04, 0x42, 0xDC, 0x48, 0x1D, 0x98, 0x1C, 0x46, 0x77, 0x56, 0x98, 0xEE, 0xD2>;

    /// No Sonar cpp:S4963 b/c `CallbackNode` supports move operation.
    ///
    class CallbackNode final : public cavl::Node<CallbackNode>  // NOSONAR cpp:S4963
    {
    public:
        CallbackNode(SingleThreadedExecutor& executor, Callback::Function&& function)
            : executor_{executor}
            , function_{std::move(function)}
            , next_exec_time_{TimePoint::max()}
            , schedule_{Callback::Schedule::None{}}
        {
            CETL_DEBUG_ASSERT(function_, "");
            executor.onCallbackHandling({}, static_cast<Callback::Handle>(*this));
        }
        ~CallbackNode()
        {
            if (auto** const root_node_ptr = getRootNodePtr())
            {
                remove(*root_node_ptr, this);
            }
            executor().onCallbackHandling(static_cast<Callback::Handle>(*this), {});
        };

        CallbackNode(CallbackNode&& other) noexcept
            : Node(std::move(static_cast<Node&>(other)))
            , executor_{other.executor_}
            , function_{std::move(other.function_)}
            , next_exec_time_{other.next_exec_time_}
            , schedule_{other.schedule_}
        {
            executor().onCallbackHandling(static_cast<Callback::Handle>(other), static_cast<Callback::Handle>(*this));
        }

        CallbackNode& operator=(CallbackNode&& other) noexcept
        {
            static_cast<Node&>(*this) = std::move(static_cast<Node&>(other));

            executor_       = other.executor_;
            function_       = std::move(other.function_);
            next_exec_time_ = other.next_exec_time_;
            schedule_       = other.schedule_;

            executor().onCallbackHandling(static_cast<Callback::Handle>(other), static_cast<Callback::Handle>(*this));

            return *this;
        }

        CallbackNode(const CallbackNode&)            = delete;
        CallbackNode& operator=(const CallbackNode&) = delete;

        explicit operator Callback::Handle() const noexcept
        {
            // Next nolint & NOSONAR are unavoidable: this is opaque handle conversion code.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            return reinterpret_cast<Callback::Handle>(this);  // NOSONAR cpp:S3630
        }
        static CallbackNode* fromHandle(const Callback::Handle handle) noexcept
        {
            CETL_DEBUG_ASSERT(handle, "");

            // Next nolint & NOSONAR are unavoidable: this is opaque handle conversion code.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
            return reinterpret_cast<CallbackNode*>(handle);  // NOSONAR cpp:S3630
        }

        SingleThreadedExecutor& executor() const noexcept
        {
            return executor_.get();
        }

        const Callback::Schedule::Variant& getSchedule() const noexcept
        {
            return schedule_;
        }

        void setSchedule(const Callback::Schedule::Variant& schedule)
        {
            schedule_ = schedule;
            cetl::visit(cetl::make_overloaded(
                            [this](const Callback::Schedule::None&) {
                                //
                                next_exec_time_ = TimePoint::max();
                            },
                            [this](const Callback::Schedule::Once& once) {
                                //
                                next_exec_time_ = once.exec_time;
                            },
                            [this](const Callback::Schedule::Repeat& repeat) {
                                //
                                next_exec_time_ = repeat.exec_time;
                            }),
                        schedule);
        }

        void reschedule(const TimePoint exec_time)
        {
            cetl::visit(cetl::make_overloaded(
                            [this](const Callback::Schedule::None&) {
                                //
                                next_exec_time_ = TimePoint::max();
                            },
                            [this](const Callback::Schedule::Once&) {
                                //
                                next_exec_time_ = TimePoint::max();
                            },
                            [this, exec_time](const Callback::Schedule::Repeat& repeat) {
                                //
                                next_exec_time_ = exec_time + repeat.period;
                            }),
                        schedule_);
        }

        TimePoint nextExecTime() const noexcept
        {
            return next_exec_time_;
        }

        void operator()(const TimePoint now_time) const
        {
            function_(now_time);
        }

        CETL_NODISCARD std::int8_t compareByExecutionTime(const TimePoint exec_time) const noexcept
        {
            // No two execution times compare equal, which allows us to have multiple nodes
            // with the same execution time in the tree. With two nodes sharing the same execution time,
            // the one added later is considered to be later.
            return (exec_time >= next_exec_time_) ? +1 : -1;
        }

        // MARK: - cetl::rtti

        static constexpr cetl::type_id _get_type_id_() noexcept
        {
            return cetl::type_id_type_value<CallbackNodeTypeIdType>();
        }

        // No Sonar `cpp:S5008` and `cpp:S5356` b/c they are unavoidable - RTTI integration.
        CETL_NODISCARD void* _cast_(const cetl::type_id& id) & noexcept  // NOSONAR cpp:S5008
        {
            return (id == _get_type_id_()) ? this : nullptr;  // NOSONAR cpp:S5356
        }

        // No Sonar `cpp:S5008` and `cpp:S5356` b/c they are unavoidable - RTTI integration.
        CETL_NODISCARD const void* _cast_(const cetl::type_id& id) const& noexcept  // NOSONAR cpp:S5008
        {
            return (id == _get_type_id_()) ? this : nullptr;  // NOSONAR cpp:S5356
        }

    private:
        // MARK: Data members:

        std::reference_wrapper<SingleThreadedExecutor> executor_;
        Callback::Function                             function_;
        TimePoint                                      next_exec_time_;
        Callback::Schedule::Variant                    schedule_;

    };  // CallbackNode

    static CallbackNode* callbackToNode(Callback::Any& callback) noexcept
    {
        return cetl::get_if<CallbackNode>(&callback);
    }

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

    void removeCallbackNode(CallbackNode& callback_node)
    {
        callback_nodes_.remove(&callback_node);
    }

    void scheduleCallbackNode(CallbackNode& callback_node, const Callback::Schedule::Variant& schedule)
    {
        removeCallbackNode(callback_node);
        callback_node.setSchedule(schedule);
        insertCallbackNode(callback_node);
    }

    // MARK: - Data members:

    cavl::Tree<CallbackNode> callback_nodes_;

};  // SingleThreadedExecutor

}  // namespace platform
}  // namespace libcyphal

#endif  // LIBCYPHAL_PLATFORM_SINGLE_THREADED_EXECUTOR_HPP_INCLUDED
