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
    /// @brief Defines a callback abstraction umbrella type.
    ///
    struct Callback
    {
        /// @brief Defines low level unique identifier for a callback.
        ///
        /// 64 bits should be enough to ensure uniqueness of the callback identifier
        /// by applying some simple rules (e.g. incrementing counter).
        ///
        using Id = std::uint64_t;

        /// @brief Defines possible variants of callback schedules.
        ///
        struct Schedule
        {
            /// @brief Defines schedule which will execute callback function at the specified execution time once.
            ///
            struct Once
            {
                /// - If `false`, the callback will stay registered and could be rescheduled again.
                /// - Otherwise, the corresponding callback will be automatically removed on its execution -
                ///   essentially auto releasing all the associated/captured resources,
                ///   and invalidating its handle (which can't be used anymore for further rescheduling).
                bool is_auto_remove{false};
            };

            /// @brief Defines schedule which will execute callback function at the specified execution time, and
            /// then repeatedly at `exec_time + (N * period)` with strict period advancement, no phase error growth.
            ///
            struct Repeat
            {
                /// @brief Positive (non-zero) period between each callback execution.
                Duration period;
            };

            using Variant = cetl::variant<Once, Repeat>;

        };  // Schedule

        /// @brief Defines move-only RAII type for automatic callback un-registration.
        ///
        /// NOSONAR cpp:S4963 for below `class Handle` - we do directly handle callback resource here.
        ///
        class Handle final  // NOSONAR cpp:S4963
        {
        public:
            /// @brief Creates default handler - it's considered as "invalid".
            ///
            /// Useful as initial value for a non-engaged callback handle.
            ///
            Handle()
                : id_{0}
                , executor_{nullptr} {};

            Handle(Handle&& other) noexcept
                : id_{std::exchange(other.id_, 0)}
                , executor_{std::exchange(other.executor_, nullptr)}
            {
            }
            Handle& operator=(Handle&& other) noexcept
            {
                reset();
                id_       = std::exchange(other.id_, 0);
                executor_ = std::exchange(other.executor_, nullptr);
                return *this;
            }

            ~Handle()
            {
                reset();
            }

            Handle(const Handle&)            = delete;
            Handle& operator=(const Handle&) = delete;

            /// @brief Gets low level unique identifier for a callback.
            ///
            Id id() const noexcept
            {
                return id_;
            }

            /// @brief Checks if this handle is valid, i.e. can be used for callback scheduling.
            ///
            /// Default constructed handle is considered as invalid b/c it has been never appended to any executor (see
            /// `IExecutor::registerCallback` method). Handle can also be invalid b/c of explicit `reset` invocation.
            ///
            constexpr explicit operator bool() const noexcept
            {
                return nullptr != executor_;
            }

            /// @brief Schedules callback (if this handle is valid) for execution at the desired absolute time.
            ///
            /// Actual execution of the callback's function will be done later (not from context of this method),
            /// when desired time comes and executor is ready to execute the callbacks.
            /// It's ok to schedule the same callback multiple times, even before previous scheduling was executed -
            /// it will be rescheduled, and then executed according to the last setup. Once it has been
            /// executed, the callback could be scheduled again (assuming it was not set up for auto-removal).
            ///
            /// @param callback_id Unique identifier of the callback to be scheduled.
            /// @param exec_time Absolute time point when it's desired to execute it.
            ///                  Use current time (aka now) to schedule it for ASAP execution.
            ///                  It could be in the past as well - callback will be also executed as ASAP.
            /// @param schedule Contains specifics how callback will be scheduled (like once, repeatedly etc.).
            /// @return `true` if this handle is valid, and its callback was found and successfully scheduled.
            ///         Otherwise `false` - in case the handle either has been reset, invalidated (f.e. by
            ///         auto removal on execution), or there are no enough resources to schedule the callback.
            ///
            bool scheduleAt(const TimePoint exec_time, const Schedule::Variant& schedule) const
            {
                if (nullptr == executor_)
                {
                    return false;
                }
                return executor_->scheduleCallbackById(id_, exec_time, schedule);
            }

            /// @brief Removes callback from the executor (if this handle is valid).
            ///
            /// This method will cancel previous scheduled (if any) execution of the callback,
            /// and also reset pointer to the executor inside - invaliding this handle.
            ///
            void reset() noexcept
            {
                if (nullptr != executor_)
                {
                    executor_->removeCallbackById(id_);
                    executor_ = nullptr;
                }
            }

        private:
            /// @brief Private constructor of a new VALID handle with the given unique identifier and executor.
            ///
            /// It is private b/c it's expected to be used only by an `IExecutor` implementation -
            /// hence the `friend class IExecutor;` below.
            ///
            /// @param id Unique identifier of the callback which was issued by the executor.
            /// @param executor Reference to the executor which has appended the callback. Pointer to the executor
            ///                 is stored inside - it will be used later for the callback scheduling and removal;
            ///                 b/c of the storing, a still valid handle must not outlive the executor.
            /// @{
            friend class IExecutor;
            ///
            Handle(const Id id, IExecutor& executor)
                : id_(id)
                , executor_(&executor)
            {
            }
            friend class IExecutor;
            /// @}

            Id         id_;
            IExecutor* executor_;

        };  // Handle

        /// @brief Defines maximum size of callback function.
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
    /// @param function The function to be called when the callback is executed.
    /// @param is_auto_remove If `true`, the callback will be automatically removed at execution.
    ///                       Useful for "one-time fire" callbacks, so that release resources as soon as fired.
    /// @return Valid handle to the successfully appended callback;
    ///         Otherwise invalid handle (see `Handle::operator bool`) - in case of the appending failure.
    ///
    CETL_NODISCARD Callback::Handle registerCallback(Callback::Function&& function)
    {
        CETL_DEBUG_ASSERT(function, "Callback function must be provided.");

        const auto opt_callback_id = appendCallback(std::move(function));
        if (opt_callback_id)
        {
            return Callback::Handle{*opt_callback_id, *this};
        }
        return {};
    }

protected:
    IExecutor() = default;

    /// @brief Appends a new callback to the executor if possible.
    ///
    /// @param A function to be called when the callback is scheduled for execution.
    /// @return A new unique identifier for the callback if successful.
    ///         Otherwise `nullopt` in case of out of memory error.
    ///         No discard b/c it's expected to be used in conjunction with `scheduleCallbackByIdAt`
    ///         or `removeCallbackById` methods.
    ///
    CETL_NODISCARD virtual cetl::optional<Callback::Id> appendCallback(Callback::Function&& function) = 0;

    /// @brief Schedules previously appended callback (by its id) for execution at the desired absolute time.
    ///
    /// Actual execution of the callback's function will be done later (not from context of this method), when desired
    /// time comes and executor is ready to execute the callbacks. It's ok to schedule the same callback multiple times
    /// even before it was executed - it will be rescheduled, and then executed according to the last setup.
    ///
    /// @param callback_id Unique identifier of the callback to be scheduled.
    /// @param time_point Absolute time point when it's desired to execute it.
    ///                   Use current time (aka now) to schedule it for ASAP execution.
    ///                   It could be in the past as well - callback will be executed as soon as possible.
    /// @param schedule Contains specifics how callback will be scheduled (like once, repeatedly etc.).
    /// @return `true` if the callback was found and successfully scheduled.
    ///         Otherwise, in case the callback has been removed already (f.e. by auto removal on execution),
    ///         or there are not enough resources to schedule the callback.
    ///
    virtual bool scheduleCallbackById(const Callback::Id                 callback_id,
                                      const TimePoint                    exec_time,
                                      const Callback::Schedule::Variant& schedule) = 0;

    /// @brief Removes callback from this executor by unique identifier.
    ///
    /// Previously already scheduled callback will not be executed.
    /// It's ok to remove already removed callback (f.e. in case of "auto removal) - it will be silently ignored.
    ///
    /// @param callback_id Unique identifier of the callback to be removed.
    ///
    virtual void removeCallbackById(const Callback::Id callback_id) = 0;

};  // IExecutor

}  // namespace libcyphal

#endif  // LIBCYPHAL_EXECUTOR_HPP_INCLUDED
