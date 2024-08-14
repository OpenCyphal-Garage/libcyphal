/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_VIRTUAL_TIME_SCHEDULER_HPP_INCLUDED
#define LIBCYPHAL_VIRTUAL_TIME_SCHEDULER_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/platform/single_threaded_executor.hpp>
#include <libcyphal/types.hpp>

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace libcyphal
{

class VirtualTimeScheduler final : public platform::SingleThreadedExecutor
{
public:
    explicit VirtualTimeScheduler(const TimePoint initial_now = {})
        : now_{initial_now}
    {
    }

    void scheduleAt(const TimePoint exec_time, Callback::Function&& function)
    {
        auto callback = registerCallback(std::move(function));
        callback.schedule(Callback::Schedule::Once{exec_time});
        callbacks_bag_.emplace_back(std::move(callback));
    }

    void scheduleAt(const Duration duration, Callback::Function&& function)
    {
        scheduleAt(TimePoint{} + duration, std::move(function));
    }

    void spinFor(const Duration duration)
    {
        const auto end_time = now_ + duration;

        while (now() < end_time)
        {
            const auto spin_result = spinOnce();
            if (testing::Test::HasFatalFailure())
            {
                break;
            }

            if (!spin_result.next_exec_time)
            {
                break;
            }

            now_ = *spin_result.next_exec_time;
        }

        now_ = end_time;
    }

    CETL_NODISCARD Callback::Any registerNamedCallback(const std::string& name, Callback::Function&& function)
    {
        auto callback              = registerCallback(std::move(function));
        named_cb_interfaces_[name] = callback.getInterface();
        return callback;
    }
    void scheduleNamedCallback(const std::string& name)
    {
        scheduleNamedCallback(name, Callback::Schedule::Once{now_});
    }
    void scheduleNamedCallback(const std::string& name, const TimePoint time_point)
    {
        scheduleNamedCallback(name, Callback::Schedule::Once{time_point});
    }
    void scheduleNamedCallback(const std::string& name, const Callback::Schedule::Variant& schedule)
    {
        named_cb_interfaces_.at(name)->schedule(schedule);
    }
    CETL_NODISCARD Callback::Any registerAndScheduleNamedCallback(const std::string&   name,
                                                                  const TimePoint      time_point,
                                                                  Callback::Function&& function)
    {
        return registerAndScheduleNamedCallback(name, Callback::Schedule::Once{time_point}, std::move(function));
    }
    CETL_NODISCARD Callback::Any registerAndScheduleNamedCallback(const std::string&                 name,
                                                                  const Callback::Schedule::Variant& schedule,
                                                                  Callback::Function&&               function)
    {
        auto callback = registerNamedCallback(name, std::move(function));
        callback.schedule(schedule);
        return callback;
    }
    CETL_NODISCARD bool hasNamedCallback(const std::string& name)
    {
        return named_cb_interfaces_.find(name) != named_cb_interfaces_.end();
    }

    // MARK: - IExecutor

    TimePoint now() const noexcept override
    {
        return now_;
    }

protected:
    void onCallbackHandling(const CallbackHandling::Variant& event_var) override
    {
        cetl::visit([this](const auto& event) { onCallbackHandlingImpl(event); }, event_var);
    }

private:
    void onCallbackHandlingImpl(const CallbackHandling::Moved& moved)
    {
        auto it = named_cb_interfaces_.begin();
        while (it != named_cb_interfaces_.end())
        {
            if (it->second == moved.old_interface)
            {
                it->second = moved.new_interface;
            }
            ++it;
        }
    }

    void onCallbackHandlingImpl(const CallbackHandling::Removed& removed)
    {
        auto it = named_cb_interfaces_.begin();
        while (it != named_cb_interfaces_.end())
        {
            if (it->second == removed.old_interface)
            {
                it = named_cb_interfaces_.erase(it);
                continue;
            }
            ++it;
        }
    }

    TimePoint                                   now_;
    std::vector<Callback::Any>                  callbacks_bag_;
    std::map<std::string, Callback::Interface*> named_cb_interfaces_;

};  // VirtualTimeScheduler

}  // namespace libcyphal

#endif  // LIBCYPHAL_VIRTUAL_TIME_SCHEDULER_HPP_INCLUDED
