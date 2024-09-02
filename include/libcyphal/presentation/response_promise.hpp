/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_RESPONSE_PROMISE_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_RESPONSE_PROMISE_HPP_INCLUDED

#include "client_impl.hpp"

#include "libcyphal/transport/scattered_buffer.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pmr/function.hpp>

#include <tuple>
#include <utility>

namespace libcyphal
{
namespace presentation
{

/// Internal implementation details of the Presentation layer.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

/// @brief Defines internal base class for any concrete (final) response promise.
///
/// No Sonar cpp:S4963 "The "Rule-of-Zero" should be followed"
/// b/c we do directly handle resources here.
///
class ResponsePromiseBase : public ClientImpl::CallbackNode  // NOSONAR cpp:S4963
{
public:
    ResponsePromiseBase(ResponsePromiseBase&& other) noexcept
        : CallbackNode{std::move(static_cast<CallbackNode&>(other))}
        , client_impl_{std::exchange(other.client_impl_, nullptr)}
    {
        CETL_DEBUG_ASSERT(client_impl_ != nullptr, "Not supposed to move from already moved `other`.");
        // No need to retain the moved object, as it is already retained.
    }

    ResponsePromiseBase(const ResponsePromiseBase& other)                = delete;
    ResponsePromiseBase& operator=(const ResponsePromiseBase& other)     = delete;
    ResponsePromiseBase& operator=(ResponsePromiseBase&& other) noexcept = delete;

protected:
    ~ResponsePromiseBase()
    {
        if (client_impl_ != nullptr)
        {
            client_impl_->releaseCallbackNode(*this);
        }
    }

    ResponsePromiseBase(ClientImpl* const client_impl, const transport::TransferId transfer_id)
        : CallbackNode{transfer_id}
        , client_impl_{client_impl}
    {
        CETL_DEBUG_ASSERT(client_impl_ != nullptr, "");

        client_impl_->retainCallbackNode(*this);
    }

private:
    // MARK: Data members:

    ClientImpl* client_impl_;

};  // ResponsePromiseBase

}  // namespace detail

/// @brief Defines promise class of a strong-typed response.
///
/// @tparam Response The response type of the promise.
///
template <typename Response>
class ResponsePromise final : public detail::ResponsePromiseBase
{
public:
    /// @brief Defines the strong-typed response and its metadata.
    ///
    using Reply = std::tuple<Response, transport::ServiceRxMetadata>;

    /// @brief Defines result of the promise. Could be either a reply or a timeout duration.
    ///
    using Result = cetl::variant<Reply, Duration>;

    /// @brief Defines the strong-typed response callback (arguments, function).
    ///
    struct Callback
    {
        struct Arg
        {
            TimePoint approx_now;
            Result    result;
        };
        using Function = cetl::pmr::function<void(const Arg&), sizeof(void*) * 4>;
    };

    /// @brief Sets function which will be called either on response reception, or on timeout.
    ///
    /// Note that setting the callback will disable the previous one (if any).
    ///
    /// @param callback_fn The function which will be called back.
    ///                    Use `nullptr` (or `{}`) to disable the callback.
    /// @param deadline    Optional deadline by which this promise will be broken,
    ///                    and as a result the callback will be called with timeout condition.
    ///
    void setCallback(typename Callback::Function&& callback_fn, const cetl::optional<TimePoint>)
    {
        callback_fn_ = std::move(callback_fn);
    }

private:
    explicit ResponsePromise(detail::ClientImpl* const client_impl, const transport::TransferId transfer_id)
        : ResponsePromiseBase{client_impl, transfer_id}
    {
    }

    // MARK: CallbackNode

    void onTimeout() noexcept override
    {
        // TODO: Implement timeout handling.
    }

    void onResponseRxTransfer(const transport::ServiceRxTransfer&) noexcept override
    {
        // TODO: Implement reply handling.
    }

    // MARK: Data members:

    cetl::optional<Result>      opt_result_;
    typename Callback::Function callback_fn_;

};  // ResponsePromise<Response>

/// @brief Defines promise class of a raw (aka untyped) response.
///
template <>
class ResponsePromise<void> final : public detail::ResponsePromiseBase
{
public:
    struct Result
    {
        struct Success
        {
            transport::ScatteredBuffer;
            transport::ServiceRxMetadata;
        };
        struct Time Failure = cetl::variant<PlatformError, ArgumentError>;

        using Type = Expected<Success, Failure>;
    };


    /// @brief Defines successful strong-typed response and its metadata.
    ///
    using Success = std::tuple<transport::ScatteredBuffer, transport::ServiceRxMetadata>;

    /// @brief Defines result of the promise. Could be either a successful reply, or a timeout duration.
    ///
    using Result = Expected<Success, Duration>;

    /// @brief Defines the strong-typed response callback (arguments, function).
    ///
    struct Callback
    {
        struct Arg
        {
            TimePoint approx_now;
            Result    result;
        };
        using Function = cetl::pmr::function<void(const Arg&), sizeof(void*) * 4>;
    };

    /// @brief Sets function which will be called either on response reception, or on timeout.
    ///
    /// Note that setting the callback will disable the previous one (if any).
    ///
    /// @param callback_fn The function which will be called back.
    ///                    Use `nullptr` (or `{}`) to disable the callback.
    /// @param deadline    Optional deadline by which this promise will be broken,
    ///                    and as a result the callback will be called with timeout condition.
    ///
    void setCallback(typename Callback::Function&& callback_fn, const cetl::optional<TimePoint>)
    {
        callback_fn_ = std::move(callback_fn);
    }

private:
    explicit ResponsePromise(detail::ClientImpl* const client_impl, const transport::TransferId transfer_id)
        : ResponsePromiseBase{client_impl, transfer_id}, was_result_{false}
    {
    }

    // MARK: CallbackNode

    void onResponseTimeout(const TimePoint approx_now) override
    {
        CETL_DEBUG_ASSERT(!was_result_, "Already received result timeout.");
        if (was_result_)
        {
            return;
        }
        was_result_ = true;

        if (callback_fn_)
        {
            auto func = std::exchange(callback_fn_, nullptr);
            func({approx_now, Duration{0}});
            return;
        }
        (void) opt_result_.emplace(Reply{std::move(transfer.payload), transfer.metadata});
    }

    void onResponseRxTransfer(const TimePoint approx_now, transport::ServiceRxTransfer& transfer) override
    {
        CETL_DEBUG_ASSERT(!was_result_, "Already received result transfer.");
        if (was_result_)
        {
            return;
        }
        was_result_ = true;

        if (callback_fn_)
        {
            auto func = std::exchange(callback_fn_, nullptr);
            func({approx_now, Reply{std::move(transfer.payload), transfer.metadata}});
            return;
        }
        (void) opt_result_.emplace(Reply{std::move(transfer.payload), transfer.metadata});
    }

    // MARK: Data members:

    bool was_result_;
    cetl::optional<Result>      opt_result_;
    typename Callback::Function callback_fn_;

};  // ResponsePromise<void>

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_RESPONSE_PROMISE_HPP_INCLUDED
