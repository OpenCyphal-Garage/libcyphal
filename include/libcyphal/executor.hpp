/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_EXECUTOR_HPP_INCLUDED
#define LIBCYPHAL_EXECUTOR_HPP_INCLUDED

#include "types.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pmr/function.hpp>
#include <cetl/rtti.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>

namespace libcyphal
{

// EBAF7312-5CFE-45F5-89FF-D9B9FE45F8EB
using IExecutorTypeIdType = cetl::
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
    type_id_type<0xEB, 0xAF, 0x73, 0x12, 0x5C, 0xFE, 0x45, 0xF5, 0x89, 0xFF, 0xD9, 0xB9, 0xFE, 0x45, 0xF8, 0xEB>;

/// @brief Defines an abstract interface for a callback executor.
///
class IExecutor : public cetl::rtti_helper<IExecutorTypeIdType>
{
public:
    /// NOSONAR cpp:S4963 - we do directly handle callback resource here.
    ///
    class Callback
    {
    public:
        /// @brief Defines possible variants of callback schedules.
        ///
        struct Schedule
        {
            /// @brief Defines schedule which will NOT execute callback function.
            ///
            /// Useful as a default value for a non-engaged callback schedule.
            /// Also can be used to cancel previously scheduled callback.
            ///
            struct None
            {};

            /// @brief Defines schedule which will execute callback function at the specified execution time once.
            ///
            struct Once
            {
                /// Absolute time point when it's desired to execute it.
                /// Use current time (aka now) to schedule it for ASAP execution.
                /// It could be in the past as well - callback will be executed as soon as possible.
                TimePoint exec_time;
            };

            /// @brief Defines schedule which will execute callback function at the specified execution time, and
            /// then repeatedly at `exec_time + (N * period)` with strict period advancement, no phase error growth.
            ///
            struct Repeat
            {
                /// Absolute time point when it's desired to execute it first time.
                TimePoint exec_time;

                /// Positive (non-zero) period between each callback execution.
                Duration period;
            };

            using Variant = cetl::variant<None, Once, Repeat>;

        };  // Schedule

        /// @brief Defines maximum size of callback function.
        ///
        /// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
        ///
        static constexpr std::size_t FunctionMaxSize = sizeof(void*) * 8;

        /// @brief Defines callback function signature.
        ///
        /// The callback function is executed from the executor's spin context (not from context of the event
        /// which has triggered callback). So, it's safe use any executor's API from a callback function.
        ///
        /// @param now_time The current time point (aka now) when the callback is really executed.
        ///                 Depending on executor load, the actual time could be a bit later
        ///                 than it was originally scheduled as desired execution time.
        ///
        using Function = cetl::pmr::function<void(const TimePoint now_time), FunctionMaxSize>;

        /// @brief Defines maximum size of callback implementation.
        ///
        /// Size is chosen arbitrary, but it should be enough to store any callback implementation.
        ///
        static constexpr std::size_t MaxSize = (sizeof(void*) * 10) + sizeof(Function);

        /// @brief Defines type-erased callback which is capable to store some internal implementation.
        ///
        /// It is supposed to be used to hide callback implementation,
        /// and to be passed to the executor for scheduling.
        /// It also manages lifetime and ownership of the callback registration, so it's movable but not copyable.
        ///
        using Any = cetl::unbounded_variant<MaxSize, false /* Copyable */, true /* Movable */>;

        /// @brief Defines opaque handle to a registered callback.
        ///
        /// It is supposed to be used internaly (by derived executors)
        /// to identify/reference the callback in the executor.
        ///
        using Handle = std::uintptr_t;

    };  // Callback

    ~IExecutor() override = default;

    IExecutor(const IExecutor&)                = delete;
    IExecutor(IExecutor&&) noexcept            = delete;
    IExecutor& operator=(const IExecutor&)     = delete;
    IExecutor& operator=(IExecutor&&) noexcept = delete;

    /// @brief Gets the current time point (aka now) of the executor.
    ///
    virtual TimePoint now() const noexcept = 0;

    /// @brief Registers a new callback by appending it to the executor.
    ///
    /// Reference to the callback will be stored in the executor and will be ready for scheduling.
    /// For scheduling, the very same executor instance must be used; otherwise, undefined behavior (UB).
    ///
    /// @param function The function to be called when the callback is executed.
    /// @return Type-erased instance of the registered callback. Instance must not outlive the executor,
    ///         and must be used only with the same executor; otherwise undefined behavior.
    ///
    CETL_NODISCARD virtual Callback::Any registerCallback(Callback::Function&& function) = 0;

    /// @brief Schedules previously registered callback for execution according to a desired schedule.
    ///
    /// Actual execution of the callback's function will be done later (not from context of this method), when desired
    /// time comes and executor is ready to execute callbacks. It's ok to schedule the same callback multiple times
    /// even before it was executed - it will be rescheduled, and then executed according to the last setup.
    ///
    /// @param callback The callback instance (registered at this executor) to be scheduled.
    ///                 There will be no scheduling if callback is not registered yet
    ///                 (in its initial default value), or has been reset already.
    /// @param schedule Contains specifics of how exactly callback will be scheduled (like once, repeatedly etc.).
    ///
    virtual void scheduleCallback(Callback::Any& callback, const Callback::Schedule::Variant& schedule) = 0;

protected:
    IExecutor() = default;

};  // IExecutor

}  // namespace libcyphal

#endif  // LIBCYPHAL_EXECUTOR_HPP_INCLUDED
