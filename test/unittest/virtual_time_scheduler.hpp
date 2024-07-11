/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_VIRTUAL_TIME_SCHEDULER_HPP
#define LIBCYPHAL_VIRTUAL_TIME_SCHEDULER_HPP

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

    TimePoint now() const
    {
        return now_;
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

    void schedule(std::function<void()> action)
    {
        scheduleAt(now_, std::move(action));
    }

    void scheduleAt(const TimePoint time_point, std::function<void()> action)
    {
        actions_.emplace(time_point, std::move(action));
    }

    void scheduleAt(const Duration duration, std::function<void()> action)
    {
        scheduleAt(TimePoint{} + duration, std::move(action));
    }

    void scheduleAfter(const Duration duration, std::function<void()> action)
    {
        scheduleAt(now_ + duration, std::move(action));
    }

    void spinFor(const Duration duration)
    {
        const auto end_time = now_ + duration;

        for (auto next = actions_.begin(); next != actions_.end(); next = actions_.begin())
        {
            const auto next_time = next->first;
            if (next_time >= end_time)
            {
                break;
            }

            auto next_action = std::move(next->second);
            actions_.erase(next);

            now_ = next_time;
            if (next_action)
            {
                next_action();

                if (testing::Test::HasFatalFailure())
                {
                    return;
                }
            }
        }

        now_ = end_time;
    }

    void reset(const TimePoint initial_now = {})
    {
        now_ = initial_now;
        actions_.clear();
    }

    // MARK: - IExecutor

private:
    TimePoint                                       now_;
    std::multimap<TimePoint, std::function<void()>> actions_;
};

}  // namespace libcyphal

#endif  // LIBCYPHAL_VIRTUAL_TIME_SCHEDULER_HPP
