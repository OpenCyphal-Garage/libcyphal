/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_CLIENT_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_CLIENT_HPP_INCLUDED

#include "client_impl.hpp"
#include "presentation_delegate.hpp"
#include "response_promise.hpp"

#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <nunavut/support/serialization.hpp>

#include <type_traits>
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

/// @brief Defines internal base class for any concrete (final) service client.
///
/// No Sonar cpp:S4963 'The "Rule-of-Zero" should be followed'
/// b/c we do directly handle resources here.
///
class ClientBase  // NOSONAR cpp:S4963
{
public:
    /// @brief Defines error type for the case when there are too many pending (aka still in progress) requests.
    ///
    /// Total number of possible pending requests is limited by the transport layer, namely by the range of
    /// possible transfer IDs. F.e. in case of CAN transport, the range is 0-31 (32 in total). For UDP transport
    /// the range is virtually unlimited (2^64), but practically limited by the available memory.
    ///
    struct TooManyPendingRequestsError
    {};

    /// @brief Defines failure type for a base client operations.
    ///
    /// The set of possible failures of the base client includes transport layer failures,
    /// as well as the `TooManyPendingRequestsError` (see docs above).
    ///
    using Failure = libcyphal::detail::AppendType<transport::AnyFailure, TooManyPendingRequestsError>::Result;

    ClientBase(const ClientBase& other)
        : shared_client_{other.shared_client_}
        , priority_{other.priority_}
    {
        CETL_DEBUG_ASSERT(other.shared_client_ != nullptr,
                          "Not supposed to copy construct from already moved `other`.");

        shared_client_->retain();
    }

    ClientBase(ClientBase&& other) noexcept
        : shared_client_{std::exchange(other.shared_client_, nullptr)}
        , priority_{other.priority_}
    {
        CETL_DEBUG_ASSERT(shared_client_ != nullptr, "Not supposed to move construct from already moved `other`.");
        // No need to retain the moved object, as it is already retained.
    }

    ClientBase& operator=(const ClientBase& other)
    {
        CETL_DEBUG_ASSERT(shared_client_ != nullptr, "Not supposed to copy assign to already moved `this`.");
        CETL_DEBUG_ASSERT(other.shared_client_ != nullptr, "Not supposed to copy assign from already moved `other`.");

        if (this != &other)
        {
            shared_client_->release();

            shared_client_ = other.shared_client_;
            priority_      = other.priority_;

            shared_client_->retain();
        }
        return *this;
    }

    ClientBase& operator=(ClientBase&& other) noexcept
    {
        CETL_DEBUG_ASSERT(shared_client_ != nullptr, "Not supposed to move construct to already moved `this`.");
        CETL_DEBUG_ASSERT(other.shared_client_ != nullptr,
                          "Not supposed to move construct from already moved `other`.");

        shared_client_->release();

        shared_client_ = std::exchange(other.shared_client_, nullptr);
        priority_      = other.priority_;

        // No need to retain the moved object, as it is already retained.
        return *this;
    }

    transport::Priority getPriority() const noexcept
    {
        return priority_;
    }

    void setPriority(const transport::Priority priority) noexcept
    {
        priority_ = priority;
    }

protected:
    ~ClientBase()
    {
        if (shared_client_ != nullptr)
        {
            shared_client_->release();
        }
    }

    explicit ClientBase(SharedClient* const shared_client)
        : shared_client_{shared_client}
        , priority_{transport::Priority::Nominal}
    {
        CETL_DEBUG_ASSERT(shared_client_ != nullptr, "");
        shared_client_->retain();
    }

    SharedClient& getSharedClient() const noexcept
    {
        CETL_DEBUG_ASSERT(shared_client_ != nullptr, "");
        return *shared_client_;
    }

private:
    // MARK: Data members:

    SharedClient*       shared_client_;
    transport::Priority priority_;

};  // ClientBase

}  // namespace detail

/// @brief Defines a custom strong-typed RPC client class.
///
/// Although the client class is not requiring specifically Nunavut tool generated request/response types,
/// it follows patterns of the tool (and has dependency on its `SerializeResult` and `bitspan` helper types),
/// so it is highly recommended to use DSDL file and the tool to generate the types.
/// Otherwise, see below requirements for the `Request` and `Response` types, as well as consult with
/// Nunavut's generated code (f.e. for the signatures of expected `serialize` and `deserialize` functions).
///
/// @tparam Request The request type of the client. This type has the following requirements:
///                 - contains `_traits_::SerializationBufferSizeBytes` constant
///                 - has freestanding `serialize` function under its namespace (so that ADL will find it)
/// @tparam Response The response type of the client. This type has the following requirements:
///                 - default constructible
///                 - contains `_traits_::ExtentBytes` constant
///                 - has freestanding `deserialize` function under its namespace (so that ADL will find it).
///
template <typename Request, typename Response>
class Client final : public detail::ClientBase
{
public:
    /// @brief Defines failure type for a strong-typed client operations.
    ///
    /// The set of possible failures includes transport layer failures (inherited from the base client),
    /// as well as serialization-related ones.
    ///
    using Failure = libcyphal::detail::AppendType<Failure, nunavut::support::Error>::Result;

    // TODO: docs
    Expected<ResponsePromise<Response>, Failure> request(const TimePoint                 request_deadline,
                                                         const Request&                  request,
                                                         const cetl::optional<TimePoint> response_deadline = {}) const
    {
        using PayloadFragment = const cetl::span<const cetl::byte>;

        // 1. Try to serialize the request to raw payload buffer.
        //
        // Next nolint b/c we use a buffer to serialize the message, so no need to zero it (and performance better).
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
        std::array<std::uint8_t, Request::_traits_::SerializationBufferSizeBytes> buffer;
        const auto buffer_size = serialize(request, buffer);
        if (!buffer_size)
        {
            return buffer_size.error();
        }
        // Next nolint & NOSONAR are currently unavoidable.
        // TODO: Eliminate `reinterpret_cast` when Nunavut supports `cetl::byte` at its `serialize`.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        PayloadFragment fragment{reinterpret_cast<cetl::byte*>(buffer.data()),  // NOSONAR cpp:S3630
                                 buffer_size.value()};
        const std::array<PayloadFragment, 1> payload{fragment};

        // 2. For request (and following response) we need to allocate a transfer ID,
        //    which will be in use to pair the request with the response.
        //
        auto& shared_client   = getSharedClient();
        auto  opt_transfer_id = shared_client.allocateTransferId();
        if (!opt_transfer_id)
        {
            return TooManyPendingRequestsError{};
        }
        const auto transfer_id = *opt_transfer_id;

        // 3. Create and register a response promise object, which will be used to handle the response.
        //    Its done specifically before sending the request, so that we will be ready to handle a response
        //    immediately, even if it happens to be received in context (during) the request sending call.
        //
        ResponsePromise<Response> response_promise{&shared_client,
                                                   transfer_id,
                                                   response_deadline.value_or(request_deadline)};
        //
        const transport::TransferTxMetadata tx_metadata{{transfer_id, getPriority()}, request_deadline};
        if (auto failure = shared_client.sendRequestPayload(tx_metadata, payload))
        {
            return libcyphal::detail::upcastVariant<Failure>(std::move(*failure));
        }

        return response_promise;
    }

private:
    friend class Presentation;  // NOLINT cppcoreguidelines-virtual-class-destructor

    explicit Client(detail::SharedClient* const shared_client)
        : ClientBase{shared_client}
    {
    }

};  // Client<Request, Response>

/// @brief Defines a service typed RPC client class.
///
/// Although the client class is not requiring specifically Nunavut tool generated service type, it follows patterns
/// of the tool, so it is highly recommended to use DSDL file and the tool to generate the client type.
/// Otherwise, see below requirements for the `Service` type, and also `Client<Request, Response>` for details.
///
/// @tparam Service The service type of the client. This type has the following requirements:
///                 - Has `_traits_::IsService` boolean constant equal `true`.
///                 - Has nested `Request` type. See `Client<Request, ...>` for details.
///                 - Has nested `Response` type. See `Client<..., Response>` for details.
///
template <typename Service, typename = std::enable_if_t<detail::IsServiceTrait<Service>::value>>
using ServiceClient = Client<typename Service::Request, typename Service::Response>;

/// @brief Defines a raw (aka untyped) RPC client class.
///
/// The client class has no requirements for the request and response data (neither any Nunavut dependencies).
/// The request/response data is passed as raw bytes (without any serialization/deserialization steps).
///
class RawServiceClient final : public detail::ClientBase
{
public:
    // TODO: docs
    Expected<ResponsePromise<void>, Failure> request(const TimePoint                    request_deadline,
                                                     const transport::PayloadFragments& request_payload,
                                                     const cetl::optional<TimePoint>    response_deadline = {}) const
    {
        // 1. For request (and following response) we need to allocate a transfer ID,
        //    which will be in use to pair the request with the response.
        //
        auto& shared_client   = getSharedClient();
        auto  opt_transfer_id = shared_client.allocateTransferId();
        if (!opt_transfer_id)
        {
            return TooManyPendingRequestsError{};
        }
        const auto transfer_id = *opt_transfer_id;

        // 2. Create and register a response promise object, which will be used to handle the response.
        //    Its done specifically before sending the request, so that we will be ready to handle a response
        //    immediately, even if it happens to be received in context (during) the request sending call.
        //
        ResponsePromise<void> response_promise{&shared_client,
                                               transfer_id,
                                               response_deadline.value_or(request_deadline)};
        //
        const transport::TransferTxMetadata tx_metadata{{transfer_id, getPriority()}, request_deadline};
        if (auto failure = shared_client.sendRequestPayload(tx_metadata, request_payload))
        {
            return libcyphal::detail::upcastVariant<Failure>(std::move(*failure));
        }

        return response_promise;
    }

private:
    friend class Presentation;

    explicit RawServiceClient(detail::SharedClient* const shared_client)
        : ClientBase{shared_client}
    {
    }

};  // RawServiceClient

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_CLIENT_HPP_INCLUDED
