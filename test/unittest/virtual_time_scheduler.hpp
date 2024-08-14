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
        NamedCallbackNode new_cb_node{*this, std::move(function), name};
        insertCallbackNode(new_cb_node);
        named_cb_interfaces_[name] = &new_cb_node;
        return {std::move(new_cb_node)};
    }
    void scheduleNamedCallback(const std::string& name) const
    {
        scheduleNamedCallback(name, Callback::Schedule::Once{now_});
    }
    void scheduleNamedCallback(const std::string& name, const TimePoint time_point) const
    {
        scheduleNamedCallback(name, Callback::Schedule::Once{time_point});
    }
    void scheduleNamedCallback(const std::string& name, const Callback::Schedule::Variant& schedule) const
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

private:
    using Self = VirtualTimeScheduler;
    using Base = SingleThreadedExecutor;

    /// No Sonar cpp:S4963 b/c `AwaitableNode` supports move operation.
    ///
    class NamedCallbackNode final : public CallbackNode  // NOSONAR cpp:S4963
    {
    public:
        NamedCallbackNode(Self& executor, Callback::Function&& function, std::string name)
            : CallbackNode{executor, std::move(function)}
            , name_{std::move(name)}
        {
        }

        ~NamedCallbackNode() override
        {
            if (!name_.empty())
            {
                getExecutor().named_cb_interfaces_.erase(name_);
            }
        }

        NamedCallbackNode(NamedCallbackNode&& other) noexcept
            : CallbackNode(std::move(static_cast<CallbackNode&>(other)))
            , name_{std::exchange(other.name_, "")}
        {
            getExecutor().named_cb_interfaces_[name_] = this;
        }

        NamedCallbackNode(const NamedCallbackNode&)                      = delete;
        NamedCallbackNode& operator=(const NamedCallbackNode&)           = delete;
        NamedCallbackNode& operator=(NamedCallbackNode&& other) noexcept = delete;

    private:
        Self& getExecutor() noexcept
        {
            return static_cast<Self&>(executor());
        }

        // MARK: Data members:
        std::string name_;

    };  // NamedCallbackNode

    TimePoint                                   now_;
    std::vector<Callback::Any>                  callbacks_bag_;
    std::map<std::string, Callback::Interface*> named_cb_interfaces_;

};  // VirtualTimeScheduler

}  // namespace libcyphal

#endif  // LIBCYPHAL_VIRTUAL_TIME_SCHEDULER_HPP_INCLUDED
