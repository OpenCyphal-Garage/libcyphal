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

/// @brief Defines internal base class for any concrete (final) response promise.
///
/// No Sonar cpp:S4963 "The "Rule-of-Zero" should be followed"
/// b/c we do directly handle resources here.
///
class ResponsePromiseBase : public detail::ClientImpl::CallbackNode  // NOSONAR cpp:S4963
{
public:
    /// @brief Defines intermediate 'waiting' state of the response promise.
    ///
    struct Waiting
    {
        /// Holds current duration of the response waiting since corresponding request time.
        Duration duration;
    };

    /// @brief Defines terminal 'expired' state of the response promise.
    ///
    struct Expired
    {
        /// Holds deadline of the expired (aka timed out) response waiting.
        TimePoint deadline;
    };

    ResponsePromiseBase(ResponsePromiseBase&& other) noexcept
        : CallbackNode{std::move(static_cast<CallbackNode&>(other))}
        , client_impl_{std::exchange(other.client_impl_, nullptr)}
        , request_time_{other.request_time_}
    {
        CETL_DEBUG_ASSERT(client_impl_ != nullptr, "Not supposed to move from already moved `other`.");
        // No need to retain the moved object, as it is already retained.
    }

    ResponsePromiseBase(const ResponsePromiseBase& other)                = delete;
    ResponsePromiseBase& operator=(const ResponsePromiseBase& other)     = delete;
    ResponsePromiseBase& operator=(ResponsePromiseBase&& other) noexcept = delete;

protected:
    ResponsePromiseBase(detail::ClientImpl* const   client_impl,
                        const transport::TransferId transfer_id,
                        const TimePoint             request_time)
        : CallbackNode{transfer_id}
        , client_impl_{client_impl}
        , request_time_{request_time}
    {
        CETL_DEBUG_ASSERT(client_impl_ != nullptr, "");

        client_impl_->retainCallbackNode(*this);
    }

    ~ResponsePromiseBase()
    {
        if (client_impl_ != nullptr)
        {
            client_impl_->releaseCallbackNode(*this);
        }
    }

    Waiting makeWaiting() const
    {
        CETL_DEBUG_ASSERT(client_impl_ != nullptr, "");

        return {client_impl_->now() - request_time_};
    }

private:
    // MARK: Data members:

    detail::ClientImpl* client_impl_;
    const TimePoint     request_time_;

};  // ResponsePromiseBase

// MARK: -

/// @brief Defines promise class of a strong-typed response.
///
/// @tparam Response The response type of the promise.
///
template <typename Response>
class ResponsePromise final : public ResponsePromiseBase
{
public:
    /// @brief Defines successful strong-typed response and its metadata.
    ///
    struct Success
    {
        Response                     response;
        transport::ServiceRxMetadata metadata;
    };

    /// @brief Defines result of the promise.
    ///
    /// Could be either a successful received response, or final expired condition.
    ///
    using Result = Expected<Success, Expired>;

private:
    explicit ResponsePromise(detail::ClientImpl* const   client_impl,
                             const transport::TransferId transfer_id,
                             const TimePoint             request_time)
        : ResponsePromiseBase{client_impl, transfer_id, request_time}
    {
    }

    // MARK: CallbackNode

    void onResponseTimeout(const TimePoint approx_now, const TimePoint deadline) override
    {
        CETL_DEBUG_ASSERT(!opt_result_, "Already received result timeout.");
        if (opt_result_)
        {
            return;
        }
        opt_result_ = Expired{deadline};

        // TODO: Implement timeout handling.
        (void) approx_now;
    }

    void onResponseRxTransfer(const TimePoint approx_now, const transport::ServiceRxTransfer& transfer) override
    {
        CETL_DEBUG_ASSERT(!opt_result_, "Already received result transfer.");
        if (opt_result_)
        {
            return;
        }

        // TODO: Implement reply handling.
        (void) approx_now;
        (void) transfer;
    }

    // MARK: Data members:

    cetl::optional<Result> opt_result_;

};  // ResponsePromise<Response>

// MARK: -

/// @brief Defines promise class of a raw (aka untyped) response.
///
template <>
class ResponsePromise<void> final : public ResponsePromiseBase
{
public:
    /// @brief Defines successful raw response and its metadata.
    ///
    struct Success
    {
        transport::ScatteredBuffer   payload;
        transport::ServiceRxMetadata metadata;
    };

    /// @brief Defines result of the promise.
    ///
    /// Could be either a successful received response, or final expired condition.
    ///
    using Result = Expected<Success, Expired>;

private:
    explicit ResponsePromise(detail::ClientImpl* const   client_impl,
                             const transport::TransferId transfer_id,
                             const TimePoint             request_time)
        : ResponsePromiseBase{client_impl, transfer_id, request_time}
    {
    }

    // MARK: CallbackNode

    void onResponseTimeout(const TimePoint approx_now, const TimePoint deadline) override
    {
        CETL_DEBUG_ASSERT(!opt_result_, "Already received result timeout.");
        if (opt_result_)
        {
            return;
        }
        opt_result_ = Expired{deadline};

        // TODO: Implement timeout handling.
        (void) approx_now;
    }

    void onResponseRxTransfer(const TimePoint approx_now, const transport::ServiceRxTransfer& transfer) override
    {
        CETL_DEBUG_ASSERT(!opt_result_, "Already received result transfer.");
        if (opt_result_)
        {
            return;
        }

        // TODO: Implement reply handling.
        (void) approx_now;
        (void) transfer;
    }

    // MARK: Data members:

    cetl::optional<Result> opt_result_;

};  // ResponsePromise<void>

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_RESPONSE_PROMISE_HPP_INCLUDED
