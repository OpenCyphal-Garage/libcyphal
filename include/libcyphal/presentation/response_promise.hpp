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

#include <nunavut/support/serialization.hpp>

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
template <typename Response>
class ResponsePromiseBase : public detail::ClientImpl::CallbackNode  // NOSONAR cpp:S4963
{
public:
    /// @brief Defines successful response and its metadata.
    ///
    struct Success
    {
        Response                     response;
        transport::ServiceRxMetadata metadata;
    };

    /// @brief Defines terminal 'expired' state of the response promise.
    ///
    struct Expired
    {
        /// Holds deadline of the expired (aka timed out) response waiting.
        TimePoint deadline;
    };

    /// @brief Defines result of the promise.
    ///
    /// Could be either a successful received response, or final expired condition.
    ///
    using Result = Expected<Success, Expired>;

    /// @brief Defines response callback (arguments, function).
    ///
    struct Callback
    {
        struct Arg
        {
            Result&   result;
            TimePoint approx_now;
        };
        using Function = cetl::pmr::function<void(const Arg&), sizeof(void*) * 4>;
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

    const cetl::optional<Expected<Success, Expired>>& get() const
    {
        return opt_result_;
    }

    cetl::optional<Expected<Success, Expired>> fetch()
    {
        if (opt_result_)
        {
            return std::move(*opt_result_);
        }
        return cetl::nullopt;
    }

    TimePoint getRequestTime() const noexcept
    {
        return request_time_;
    }

protected:
    ResponsePromiseBase(detail::ClientImpl* const   client_impl,
                        const TimePoint             request_time,
                        const transport::TransferId transfer_id,
                        const TimePoint             request_deadline)
        : CallbackNode{transfer_id, request_deadline}
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

    cetl::pmr::memory_resource& memory() const noexcept
    {
        CETL_DEBUG_ASSERT(client_impl_ != nullptr, "");
        return client_impl_->memory();
    }

    void acceptResult(Result&& result, const TimePoint approx_now)
    {
        CETL_DEBUG_ASSERT(!opt_result_, "Already received result.");
        if (opt_result_)
        {
            return;
        }
        opt_result_ = std::move(result);

        if (callback_fn_)
        {
            const auto                   callback_fn = std::exchange(callback_fn_, nullptr);
            const typename Callback::Arg arg{*opt_result_, approx_now};
            callback_fn(arg);
        }
    }

    void acceptCallback(typename Callback::Function callback_fn)
    {
        if (opt_result_)
        {
            const typename Callback::Arg arg{*opt_result_, client_impl_->now()};
            callback_fn(arg);
            return;
        }

        callback_fn_ = std::move(callback_fn);
    }

    // MARK: CallbackNode

    void onResponseTimeout(const TimePoint deadline, const TimePoint approx_now) override
    {
        acceptResult(Expired{deadline}, approx_now);
    }

private:
    // MARK: Data members:

    detail::ClientImpl*                        client_impl_;
    const TimePoint                            request_time_;
    typename Callback::Function                callback_fn_;
    cetl::optional<Expected<Success, Expired>> opt_result_;

};  // ResponsePromiseBase

// MARK: -

/// @brief Defines promise class of a strong-typed response.
///
/// @tparam Response Deserializable response type of the promise.
///
template <typename Response>
class ResponsePromise final : public ResponsePromiseBase<Response>
{
    using Base = ResponsePromiseBase<Response>;
    using Base::memory;
    using Base::acceptResult;
    using Base::acceptCallback;

public:
    using typename Base::Result;
    using typename Base::Success;
    using typename Base::Expired;
    using typename Base::Callback;

    ResponsePromise& setCallback(typename Callback::Function&& callback_fn)
    {
        acceptCallback(std::move(callback_fn));
        return *this;
    }

private:
    template <typename Request, typename Response_>
    friend class Client;
    using Base::Base;

    // No Sonar `cpp:S5356` and `cpp:S5357` b/c of raw PMR memory allocation,
    // as well as data pointer type mismatches between scattered buffer and `const_bitspan`.
    // TODO: Eliminate PMR allocation - when `deserialize` will support scattered buffers.
    // TODO: Move this method to some common place (`SubscriberImpl` and `ServerImpl` also have it).
    bool tryDeserialize(Response& out_response, const transport::ScatteredBuffer& buffer)
    {
        // Make a copy of the scattered buffer into a single contiguous temp buffer.
        //
        // Strictly speaking, we could eliminate PMR allocation here in favor of a fixed-size stack buffer
        // (`Message::_traits_::ExtentBytes`), but this might be dangerous in case of large messages.
        // Maybe some kind of hybrid approach would be better,
        // e.g. stack buffer for small messages and PMR for large ones.
        //
        const std::unique_ptr<cetl::byte, PmrRawBytesDeleter>
            tmp_buffer{static_cast<cetl::byte*>(memory().allocate(buffer.size())),  // NOSONAR cpp:S5356 cpp:S5357
                       {buffer.size(), &memory()}};
        if (!tmp_buffer)
        {
            return false;
        }
        const auto data_size = buffer.copy(0, tmp_buffer.get(), buffer.size());

        const auto* const data_raw = static_cast<const void*>(tmp_buffer.get());
        const auto* const data_u8s = static_cast<const std::uint8_t*>(data_raw);  // NOSONAR cpp:S5356 cpp:S5357
        const nunavut::support::const_bitspan bitspan{data_u8s, data_size};

        return deserialize(out_response, bitspan);
    }

    // MARK: CallbackNode

    void onResponseRxTransfer(transport::ServiceRxTransfer& transfer, const TimePoint approx_now) override
    {
        Success success{Response{}, transfer.metadata};
        if (!tryDeserialize(success.response, transfer.payload))
        {
            return;
        }

        acceptResult(std::move(success), approx_now);
    }

};  // ResponsePromise<Response>

// MARK: -

/// @brief Defines promise class of a raw (aka untyped) response.
///
template <>
class ResponsePromise<void> final : public ResponsePromiseBase<transport::ScatteredBuffer>
{
    using Base = ResponsePromiseBase;

public:
    using Base::Result;
    using Base::Success;
    using Base::Expired;
    using Base::Callback;

    ResponsePromise& setCallback(Callback::Function&& callback_fn)
    {
        acceptCallback(std::move(callback_fn));
        return *this;
    }

private:
    friend class RawServiceClient;
    using Base::Base;

    // MARK: CallbackNode

    void onResponseRxTransfer(transport::ServiceRxTransfer& transfer, const TimePoint approx_now) override
    {
        acceptResult(Success{std::move(transfer.payload), transfer.metadata}, approx_now);
    }

};  // ResponsePromise<void>

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_RESPONSE_PROMISE_HPP_INCLUDED
