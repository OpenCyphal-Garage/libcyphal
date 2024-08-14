/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_EXECUTOR_HPP_INCLUDED
#define LIBCYPHAL_EXECUTOR_HPP_INCLUDED

#include "time_provider.hpp"
#include "types.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pmr/function.hpp>
#include <cetl/rtti.hpp>
#include <cetl/unbounded_variant.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>

namespace libcyphal
{

/// @brief Defines an abstract interface for a callback executor.
///
class IExecutor : public ITimeProvider
{
    // EBAF7312-5CFE-45F5-89FF-D9B9FE45F8EB
    // clang-format off
    using TypeIdType = cetl::type_id_type<
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
        0xEB, 0xAF, 0x73, 0x12, 0x5C, 0xFE, 0x45, 0xF5, 0x89, 0xFF, 0xD9, 0xB9, 0xFE, 0x45, 0xF8, 0xEB>;
    // clang-format on

public:
    /// @brief Defines umbrella type for everything callback related.
    ///
    struct Callback
    {
        /// @brief Defines possible variants of callback schedules.
        ///
        struct Schedule
        {
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

            using Variant = cetl::variant<Once, Repeat>;

        };  // Schedule

        /// @brief Defines maximum size of callback function.
        ///
        /// Size is chosen arbitrary, but it should be enough to store any lambda or function pointer.
        ///
        static constexpr std::size_t FunctionMaxSize = sizeof(void*) * 8;

        /// @brief Defines type of callback `Function` single argument.
        ///
        /// References to instances of this type are passed to the function during callback execution.
        /// The following holds: `exec_time` <= `approx_now` <= `Executor::now()`.
        ///
        struct Arg
        {
            /// Time when the callback was scheduled to be executed.
            TimePoint exec_time;

            /// An approximation of the current time.
            TimePoint approx_now;

        };  // Arg

        /// @brief Defines callback function signature.
        ///
        /// The callback function is executed from the executor's spin context (not from context of the event
        /// which has triggered callback). So, it's safe use any executor's API from a callback function.
        ///
        /// @param now_time The current time point (aka now) when the callback is really executed.
        ///                 Depending on executor load, the actual time could be a bit later
        ///                 than it was originally scheduled as desired execution time.
        ///
        using Function = cetl::pmr::function<void(const Arg& arg), FunctionMaxSize>;

        /// @brief Defines maximum size of callback implementation.
        ///
        /// Size is chosen arbitrary, but it should be enough to store any callback implementation.
        ///
        static constexpr std::size_t MaxSize = (sizeof(void*) * 16) + sizeof(Function);

        class Interface
        {
            // 5E16E6BC-C7EB-42EC-8A98-06189C2F0349
            // clang-format off
            using TypeIdType = cetl::type_id_type<
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
                0x5E, 0x16, 0xE6, 0xBC, 0xC7, 0xEB, 0x42, 0xEC, 0x8A, 0x98, 0x06, 0x18, 0x9C, 0x2F, 0x03, 0x49>;
            // clang-format on

        public:
            Interface(const Interface&)                = delete;
            Interface(Interface&&) noexcept            = delete;
            Interface& operator=(const Interface&)     = delete;
            Interface& operator=(Interface&&) noexcept = delete;

            /// @brief Schedules previously registered callback for execution according to a desired schedule.
            ///
            /// Actual execution of the callback's function will be done later (not from context of this method),
            /// when desired time comes and executor is ready to execute callbacks. It's ok to schedule the same
            /// callback multiple times even before it was executed - it will be rescheduled, and then executed
            /// according to the last setup.
            ///
            /// @param schedule Contains specifics of how exactly callback will be scheduled.
            ///
            virtual void schedule(const Schedule::Variant& schedule) = 0;

            // MARK: RTTI

            static constexpr cetl::type_id _get_type_id_() noexcept
            {
                return cetl::type_id_type_value<TypeIdType>();
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

        protected:
            Interface()  = default;
            ~Interface() = default;

        };  // Interface

        /// @brief Defines type-erased callback which is capable to store some internal implementation.
        ///
        /// It is expected that the internal implementation supports `Callback::Interface` (RTTI-cast-able to).
        ///
        /// It is supposed to be used to hide callback implementation.
        /// It also manages lifetime and ownership of the callback registration, so it's movable but not copyable.
        ///
        /// No Sonar cpp:S110 b/c `unbounded_variant` has initial
        /// inheritance hierarchy depth of 5 (b/c it's policy-based implementation).
        ///
        class Any final  // NOSONAR cpp:S110
            : public cetl::unbounded_variant<MaxSize, false /* Copyable */, true /* Movable */>
        {
        public:
            using unbounded_variant::unbounded_variant;

            explicit operator bool() const noexcept
            {
                return has_value();
            }

            /// @breif Gets the internal implementation interface of the callback.
            ///
            Interface* getInterface() noexcept
            {
                if (!has_value())
                {
                    return nullptr;
                }

                auto* const interface = cetl::get_if<Interface>(this);
                CETL_DEBUG_ASSERT(interface != nullptr,
                                  "Internal implementation must be rtti-cast-able to `Callback::Interface`");
                return interface;
            }

            /// @brief Schedules previously registered callback for execution according to a desired schedule.
            ///
            /// Actual execution of the callback's function will be done later (not from context of this method),
            /// when desired time comes and executor is ready to execute callbacks. It's ok to schedule the same
            /// callback multiple times even before it was executed - it will be rescheduled, and then executed
            /// according to the last setup.
            ///
            /// @param schedule Contains specifics of how exactly callback will be scheduled.
            /// @return `true` if scheduling was successful, `false` otherwise (b/c callback had been reset).
            ///
            bool schedule(const Schedule::Variant& schedule)
            {
                if (auto* const interface = getInterface())
                {
                    interface->schedule(schedule);
                    return true;
                }
                return false;
            }

        };  // Any

    };  // Callback

    IExecutor(const IExecutor&)                = delete;
    IExecutor(IExecutor&&) noexcept            = delete;
    IExecutor& operator=(const IExecutor&)     = delete;
    IExecutor& operator=(IExecutor&&) noexcept = delete;

    /// @brief Registers a new callback by appending it to the executor.
    ///
    /// Reference to the callback will be stored in the executor and will be ready for scheduling.
    /// For scheduling, the very same executor instance must be used; otherwise, undefined behavior (UB).
    ///
    /// @param function The function to be called when the callback is executed.
    /// @return Type-erased instance of the registered callback.
    ///         Instance must not outlive the executor; otherwise undefined behavior.
    ///
    CETL_NODISCARD virtual Callback::Any registerCallback(Callback::Function&& function) = 0;

    // MARK: RTTI

    static constexpr cetl::type_id _get_type_id_() noexcept
    {
        return cetl::type_id_type_value<TypeIdType>();
    }

    // No Sonar `cpp:S5008` and `cpp:S5356` b/c they are unavoidable - RTTI integration.
    CETL_NODISCARD virtual void* _cast_(const cetl::type_id& id) & noexcept  // NOSONAR cpp:S5008
    {
        return (id == _get_type_id_()) ? this : nullptr;  // NOSONAR cpp:S5356
    }

    // No Sonar `cpp:S5008` and `cpp:S5356` b/c they are unavoidable - RTTI integration.
    CETL_NODISCARD virtual const void* _cast_(const cetl::type_id& id) const& noexcept  // NOSONAR cpp:S5008
    {
        return (id == _get_type_id_()) ? this : nullptr;  // NOSONAR cpp:S5356
    }

protected:
    IExecutor()  = default;
    ~IExecutor() = default;

};  // IExecutor

}  // namespace libcyphal

#endif  // LIBCYPHAL_EXECUTOR_HPP_INCLUDED
