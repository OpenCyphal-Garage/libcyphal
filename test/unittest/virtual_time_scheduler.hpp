/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_VIRTUAL_TIME_SCHEDULER_HPP_INCLUDED
#define LIBCYPHAL_VIRTUAL_TIME_SCHEDULER_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/types.hpp>

#include <gtest/gtest.h>

#include <functional>
#include <map>
#include <utility>

namespace libcyphal
{

class VirtualTimeScheduler final : public IExecutor
{
public:
    explicit VirtualTimeScheduler(const TimePoint initial_now = {})
        : now_{initial_now}
    {
    }

    void setNow(const TimePoint now)
    {
        now_ = now;
    }

    void runNow(const Duration duration)
    {
        now_ += duration;
    }

    template <typename Action>
    void runNow(const Duration duration, Action action)
    {
        runNow(duration);
        action();
    }

    void scheduleAt(const TimePoint exec_time, std::function<void()> action)
    {
        const auto opt_callback_id = appendCallback([action = std::move(action)](const TimePoint) { action(); });
        if (opt_callback_id)
        {
            const bool is_scheduled = scheduleCallbackById(  //
                *opt_callback_id,
                exec_time,
                Callback::Schedule::Once{true /*is_auto_remove*/});
            (void) is_scheduled;
            CETL_DEBUG_ASSERT(is_scheduled, "Unexpected failure to schedule callback by id.");
        }
    }

    void scheduleAt(const Duration duration, std::function<void()> action)
    {
        scheduleAt(TimePoint{} + duration, std::move(action));
    }

    Callback::Handle scheduleCallbackAfter(const Duration duration, IExecutor::Callback::Function function)
    {
        auto handle = registerCallback(std::move(function));

        const bool is_scheduled = handle.scheduleAt(now_ + duration, Callback::Schedule::Once{true /*is_auto_remove*/});
        (void) is_scheduled;
        CETL_DEBUG_ASSERT(is_scheduled, "Unexpected failure to schedule callback by id.");

        return handle;
    }

    void spinFor(const Duration duration)
    {
        const auto end_time = now_ + duration;

        for (auto next = callbacks_to_execute_.begin();  //
             next != callbacks_to_execute_.end();
             next = callbacks_to_execute_.begin())
        {
            const auto next_time = next->first;
            if (next_time >= end_time)
            {
                break;
            }

            const auto next_callback_id = next->second;
            callbacks_to_execute_.erase(next);

            const auto next_callback = callback_id_to_state_.find(next_callback_id);
            if (next_callback == callback_id_to_state_.end())
            {
                continue;
            }
            CETL_DEBUG_ASSERT(next_callback->second.is_triggered, "Callback must be triggered.");

            now_                               = next_time;
            next_callback->second.is_triggered = false;

            next_callback->second.function(next_time);
            if (testing::Test::HasFatalFailure())
            {
                return;
            }
        }

        now_ = end_time;
    }

    void reset(const TimePoint initial_now = {})
    {
        now_ = initial_now;
        callbacks_to_execute_.clear();
        callback_id_to_state_.clear();
    }

    // MARK: - IExecutor

    TimePoint now() const noexcept override
    {
        return now_;
    }

    using IExecutor::registerCallback;

    bool scheduleCallbackById(const Callback::Id                 callback_id,
                              const TimePoint                    exec_time,
                              const Callback::Schedule::Variant& schedule) override
    {
        const auto it = callback_id_to_state_.find(callback_id);
        if (it == callback_id_to_state_.end())
        {
            return false;
        }

        it->second.schedule = schedule;

        if (!it->second.is_triggered)
        {
            it->second.is_triggered = true;
            callbacks_to_execute_.emplace(exec_time, it->first);
        }
        return true;
    }

protected:
    cetl::optional<Callback::Id> appendCallback(Callback::Function&& function) override
    {
        const Callback::Id callback_id = next_callback_id_++;
        callback_id_to_state_.emplace(callback_id, CallbackState{std::move(function), false, cetl::nullopt});
        return cetl::optional<Callback::Id>{callback_id};
    }

    void removeCallbackById(const Callback::Id callback_id) override
    {
        callback_id_to_state_.erase(callback_id);
    }

private:
    struct CallbackState
    {
        Callback::Function                          function;
        bool                                        is_triggered;
        cetl::optional<Callback::Schedule::Variant> schedule;
    };

    TimePoint                              now_;
    Callback::Id                           next_callback_id_{0};
    std::multimap<TimePoint, Callback::Id> callbacks_to_execute_;
    std::map<Callback::Id, CallbackState>  callback_id_to_state_;

};  // VirtualTimeScheduler

}  // namespace libcyphal

#endif  // LIBCYPHAL_VIRTUAL_TIME_SCHEDULER_HPP_INCLUDED
