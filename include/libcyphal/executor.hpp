/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_EXECUTOR_HPP
#define LIBCYPHAL_EXECUTOR_HPP

#include "transport/errors.hpp"
#include "types.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pmr/function.hpp>

#include <cstddef>
#include <utility>

namespace libcyphal
{

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
        class Handle
        {
        public:
            Handle() noexcept = default;
            Handle(Handle&& other) noexcept
            {
                moveFrom(other);
            }
            Handle& operator=(Handle&& other) noexcept
            {
                moveFrom(other);
                return *this;
            }

            ~Handle()
            {
                reset();
            }

            Handle(const Handle&)            = delete;
            Handle& operator=(const Handle&) = delete;

            Id id() const noexcept
            {
                return id_;
            }

            void reset() noexcept
            {
                if (nullptr != executor_)
                {
                    executor_->removeCallbackById(id_);
                    executor_ = nullptr;
                }
            }

        private:
            friend class IExecutor;

            Handle(Id id, IExecutor& executor)
                : id_(id)
                , executor_(&executor)
            {
            }

            void moveFrom(Handle& other) noexcept
            {
                reset();

                id_       = other.id_;
                executor_ = std::exchange(other.executor_, nullptr);
            }

            Id         id_{0};
            IExecutor* executor_{nullptr};

        };  // Handle

        /// @brief Defines maximum size of callback function.
        ///
        constexpr static std::size_t FunctionMaxSize = sizeof(void*) * 8;

        /// @brief Defines callback function signature.
        ///
        /// The callback function is executed from the executor's spin context (not from context of an event
        /// which has triggered callback. So, it's safe use any executor's API from a callback function.
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

    /// @brief Registers a new callback at the executor.
    ///
    /// @param function The function to be called when the callback is executed.
    /// @param is_auto_remove If `true`, the callback will be automatically removed at execution.
    /// @return Handle to the successfully registered callback; otherwise `nullopt`.
    ///
    CETL_NODISCARD cetl::optional<Callback::Handle> registerCallback(Callback::Function function,
                                                                     const bool         is_auto_remove = false)
    {
        CETL_DEBUG_ASSERT(function, "Callback function must be provided.");

        cetl::optional<Callback::Handle> result;

        // No "auto remove" b/c result `Handle` is expected to manage callback lifetime.
        //
        auto callback_id = appendCallback(is_auto_remove, std::move(function));
        if (callback_id.has_value())
        {
            result = Callback::Handle{callback_id.value(), *this};
        }

        return result;
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
    ///
    virtual CETL_NODISCARD cetl::optional<Callback::Id> appendCallback(const bool         is_auto_remove,
                                                                       Callback::Function function) = 0;

    /// @brief Schedules previously appended callback (by its id) for execution at the desired absolute time.
    ///
    /// Actual execution of the callback's function will be done later (not from context of this method), when desired
    /// time comes and executor is ready to execute the callbacks. It's ok to trigger the same callback multiple times
    /// even before it was executed - it will be rescheduled, and then executed only once according to the last config.
    ///
    /// @param callback_id Unique identifier of the callback to be triggered.
    /// @param time_point Absolute time point when it's desired to execute it.
    ///                   Use current time (aka now) to schedule it for ASAP execution.
    /// @return `true` if the callback was found and triggered.
    ///
    virtual bool scheduleCallbackByIdAt(const Callback::Id callback_id, const TimePoint time_point) = 0;

    /// @brief Removes callback from this executor by unique identifier.
    ///
    /// Previously already scheduled callback will not be executed.
    ///
    /// @param callback_id Unique identifier of the callback to be removed.
    /// @return `true` if the callback was found and removed.
    ///
    virtual bool removeCallbackById(const Callback::Id callback_id) = 0;

};  // IExecutor

}  // namespace libcyphal

#endif  // LIBCYPHAL_EXECUTOR_HPP
