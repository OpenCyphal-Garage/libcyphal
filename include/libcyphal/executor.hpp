/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_EXECUTOR_HPP
#define LIBCYPHAL_EXECUTOR_HPP

#include "types.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pmr/function.hpp>

#include <cstddef>
#include <utility>

namespace libcyphal
{

/// @brief Defines an abstract interface for a callback executor.
///
class IExecutor
{
public:
    /// @brief Defines a callback abstraction umbrella type.
    ///
    struct Callback
    {
        /// @brief Defines low level unique identifier for a callback.
        ///
        using Id = std::size_t;

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
            /// it will be rescheduled, and then executed only once according to the last setup. Once it has been
            /// executed, the callback could be scheduled again (assuming it was not set up for auto-removal).
            ///
            /// @param callback_id Unique identifier of the callback to be scheduled.
            /// @param time_point Absolute time point when it's desired to execute it.
            ///                   Use current time (aka now) to schedule it for ASAP execution.
            /// @return `true` if this handle is valid, and the callback was found (not removed yet) and scheduled.
            ///
            bool scheduleAt(const TimePoint time_point) const
            {
                if (nullptr == executor_)
                {
                    return false;
                }
                return executor_->scheduleCallbackByIdAt(id_, time_point);
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
        constexpr static std::size_t FunctionMaxSize = sizeof(void*) * 8;

        /// @brief Defines callback function signature.
        ///
        /// The callback function is executed from the executor's spin context (not from context of the event
        /// which has triggered callback). So, it's safe use any executor's API from a callback function.
        ///
        /// @param time_point The current time point (aka now) when the callback is executed. Depending on executor
        ///                   load, the actual time point could be a bit later than when it was originally triggered.
        ///
        using Function = cetl::pmr::function<void(const TimePoint time_point), FunctionMaxSize>;

    };  // Callback

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
    ///         Otherwise invalid handle (see `Handle::isValid`) - in case of the appending failure.
    ///
    CETL_NODISCARD Callback::Handle registerCallback(Callback::Function function, const bool is_auto_remove = false)
    {
        CETL_DEBUG_ASSERT(function, "Callback function must be provided.");

        const auto opt_callback_id = appendCallback(is_auto_remove, std::move(function));
        if (opt_callback_id)
        {
            return Callback::Handle{*opt_callback_id, *this};
        }
        return {};
    }

protected:
    IExecutor()  = default;
    ~IExecutor() = default;

    /// @brief Appends a new callback to the executor if possible.
    ///
    /// @param is_auto_remove If `true`, the callback will be automatically removed after its execution.
    /// @param A function to be called when the callback is scheduled for execution.
    /// @return A new unique identifier for the callback if successful.
    ///         Otherwise `nullopt` in case of out of memory error.
    ///         No discard b/c it's expected to be used in conjunction with `scheduleCallbackByIdAt`
    ///         or `removeCallbackById` methods.
    ///
    CETL_NODISCARD virtual cetl::optional<Callback::Id> appendCallback(const bool         is_auto_remove,
                                                                       Callback::Function function) = 0;

    /// @brief Schedules previously appended callback (by its id) for execution at the desired absolute time.
    ///
    /// Actual execution of the callback's function will be done later (not from context of this method), when desired
    /// time comes and executor is ready to execute the callbacks. It's ok to schedule the same callback multiple times
    /// even before it was executed - it will be rescheduled, and then executed only once according to the last setup.
    ///
    /// @param callback_id Unique identifier of the callback to be scheduled.
    /// @param time_point Absolute time point when it's desired to execute it.
    ///                   Use current time (aka now) to schedule it for ASAP execution.
    /// @return `true` if the callback was found and scheduled.
    ///
    virtual bool scheduleCallbackByIdAt(const Callback::Id callback_id, const TimePoint time_point) = 0;

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

#endif  // LIBCYPHAL_EXECUTOR_HPP
