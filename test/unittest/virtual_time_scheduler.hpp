/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_VIRTUAL_TIME_SCHEDULER_HPP
#define LIBCYPHAL_VIRTUAL_TIME_SCHEDULER_HPP

#include <libcyphal/types.hpp>

namespace libcyphal
{

struct VirtualTimeScheduler final
{
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

private:
    TimePoint now_;
};

} // namespace libcyphal

#endif  // LIBCYPHAL_VIRTUAL_TIME_SCHEDULER_HPP
