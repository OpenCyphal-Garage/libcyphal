/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_VIRTUAL_TIME_SCHEDULER_HPP
#define LIBCYPHAL_VIRTUAL_TIME_SCHEDULER_HPP

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

    void scheduleAt(const TimePoint time_point, std::function<void()> action)
    {
        const auto opt_callback_id =
            appendCallback(true /*is_auto_remove*/, [action = std::move(action)](const TimePoint) { action(); });
        if (opt_callback_id)
        {
            const bool is_scheduled = scheduleCallbackByIdAt(*opt_callback_id, time_point);
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
        auto handle = registerCallback(std::move(function), true /*is_auto_remove*/);

        const bool is_scheduled = handle.scheduleAt(now_ + duration);
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

            const auto next_callback = callback_ids_to_funcs_.find(next_callback_id);
            if (next_callback == callback_ids_to_funcs_.end())
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
        callback_ids_to_funcs_.clear();
    }

    // MARK: - IExecutor

    TimePoint now() const noexcept override
    {
        return now_;
    }

    using IExecutor::registerCallback;

    bool scheduleCallbackByIdAt(const Callback::Id callback_id, const TimePoint time_point) override
    {
        const auto it = callback_ids_to_funcs_.find(callback_id);
        if (it == callback_ids_to_funcs_.end())
        {
            return false;
        }

        if (!it->second.is_triggered)
        {
            it->second.is_triggered = true;
            callbacks_to_execute_.emplace(time_point, it->first);
        }
        return true;
    }

protected:
    cetl::optional<Callback::Id> appendCallback(const bool is_auto_remove, Callback::Function function) override
    {
        const Callback::Id callback_id = next_callback_id_++;
        callback_ids_to_funcs_.emplace(callback_id, CallbackState{std::move(function), false, is_auto_remove});
        return cetl::optional<Callback::Id>{callback_id};
    }

    void removeCallbackById(const Callback::Id callback_id) override
    {
        callback_ids_to_funcs_.erase(callback_id);
    }

private:
    struct CallbackState
    {
        Callback::Function function;
        bool               is_triggered;
        bool               is_auto_remove;
    };

    TimePoint                              now_;
    Callback::Id                           next_callback_id_{0};
    std::multimap<TimePoint, Callback::Id> callbacks_to_execute_;
    std::map<Callback::Id, CallbackState>  callback_ids_to_funcs_;

};  // VirtualTimeScheduler

}  // namespace libcyphal

#endif  // LIBCYPHAL_VIRTUAL_TIME_SCHEDULER_HPP
