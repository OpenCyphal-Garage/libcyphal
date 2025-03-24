/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_CLIENT_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_CLIENT_HPP_INCLUDED

#include "client_impl.hpp"
#include "common_helpers.hpp"
#include "presentation_delegate.hpp"
#include "response_promise.hpp"

#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

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
class ClientBase
{
public:
    /// @brief Defines the error type for the case when there are too many pending (aka still in progress) requests.
    ///
    /// The total number of possible pending requests is limited by the transport layer, namely by the range of
    /// possible transfer IDs. For example, in the case of CAN transport, the range is 0-31 (32 in total).
    /// For UDP transport, the range is virtually unlimited (2^64), but practically limited by the available memory.
    ///
    struct TooManyPendingRequestsError
    {};

    /// @brief Defines a failure type for a base client operations.
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
            (void) shared_client_->release();

            shared_client_ = other.shared_client_;
            priority_      = other.priority_;

            shared_client_->retain();
        }
        return *this;
    }

    ClientBase& operator=(ClientBase&& other) noexcept
    {
        CETL_DEBUG_ASSERT(other.shared_client_ != nullptr,
                          "Not supposed to move construct from already moved `other`.");

        if (nullptr != shared_client_)
        {
            (void) shared_client_->release();
        }

        shared_client_ = std::exchange(other.shared_client_, nullptr);
        priority_      = other.priority_;

        // No need to retain the moved object, as it is already retained.
        return *this;
    }

    /// @brief Gets current priority of requests of this client.
    ///
    /// The priority is used to determine the order of the requests in the transport layer.
    ///
    transport::Priority getPriority() const noexcept
    {
        return priority_;
    }

    /// @brief Sets the priority of requests of this client.
    ///
    /// The priority is used to determine the order of the requests in the transport layer.
    /// It can be changed at any time, and the new priority will be used for the next request.
    /// Prior requests will not be affected by this change.
    ///
    /// @param priority The new priority to be set.
    ///
    void setPriority(const transport::Priority priority) noexcept
    {
        priority_ = priority;
    }

protected:
    ~ClientBase()
    {
        if (shared_client_ != nullptr)
        {
            (void) shared_client_->release();
        }
    }

    explicit ClientBase(SharedClient* const shared_client)
        : shared_client_{shared_client}
        , priority_{transport::Priority::Nominal}
    {
        CETL_DEBUG_ASSERT(shared_client_ != nullptr, "");
        shared_client_->retain();
    }

    cetl::pmr::memory_resource& memory() const noexcept
    {
        return shared_client_->memory();
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
/// Although the client class does not specifically require the Nunavut tool generated request/response types,
/// it follows patterns of the tool (and has dependency on its `SerializeResult` and `bitspan` helper types),
/// so it is highly recommended to use DSDL file and the tool to generate the types.
/// Otherwise, see below requirements for the `Request` and `Response` types, as well as consult with
/// Nunavut's generated code (f.e. for the signatures of expected `serialize` and `deserialize` functions).
///
/// @tparam Request The request type of the client. This type has the following requirements:
///                 - contains `_traits_::SerializationBufferSizeBytes` constant
///                 - has freestanding `serialize` function under its namespace (so that ADL will find it)
/// @tparam Response The response type of the client. This type has the following requirements:
///                 - contains nested `allocator_type`, which is a PMR allocator
///                 - constructible with the PMR allocator
///                 - contains `_traits_::ExtentBytes` constant
///                 - has freestanding `deserialize` function under its namespace (so that ADL will find it).
///
template <typename Request, typename Response>
class Client final : public detail::ClientBase
{
public:
    /// @brief Defines failure type for a strong-typed client operations.
    ///
    /// The set of possible failures includes transport layer failures (inherited from the base client)
    /// as well as serialization-related ones.
    ///
    using Failure = libcyphal::detail::AppendType<Failure, nunavut::support::Error>::Result;

    /// @brief Initiates a strong-typed request to the server, and returns a promise object to handle the response.
    ///
    /// If `BufferSize` is less or equal to `config::presentation::SmallPayloadSize`,
    /// the message will be serialized using a stack-allocated buffer; otherwise, PMR allocation will be used.
    ///
    /// Issuing a new request involves the following steps:
    /// 1. Serialize the request object to a raw payload buffer, which might fail with `nunavut::support::Error`.
    /// 2. Allocation of the next transfer ID not in use, so that request and response can be paired. Depending on
    ///    the transport layer (UDP, CAN, etc.), this operation might be O(1) complexity (like for UDP transport,
    ///    where range of transfer ids is 2^64 huge, so simple increment is in use to generate next "unique" id),
    ///    OR it could take O(N) complexity in the worst-case (where N is the number of pending requests), like for
    ///    CAN transport, where N is limited by 2^5. Such limited range of CAN transfer ids is the cause of possible
    ///    `TooManyPendingRequestsError` failure to allocate a new not in use id.
    /// 3. Creation and registration of a response promise object, which will be used to handle the raw response, from
    ///    the server, try to deserialize it to the strong-typed response, and deliver end result to the user.
    /// 4. Sending the raw request payload to the server, which might fail with a transport layer error.
    ///    If it does fail, then the response promise object will be destroyed, and the user will get the failure.
    ///
    /// @tparam BufferSize The size of the buffer to serialize the request.
    /// @param request_deadline The deadline for the request sending operation. The request will be dropped if not sent
    ///                         before this deadline, which will inevitably time out the response waiting deadline.
    /// @param request The request object to be serialized and then sent to the server.
    /// @param response_deadline The deadline for the response receiving operation. If `nullopt` (or `{}`) then
    ///                          `request_deadline` will be used for both request and response deadlines.
    /// @return If request sending has succeeded, then the result will be a promise object to handle the response,
    ///         which will be filled in the future with a received response. See `ResponsePromise` for details.
    ///         If request sending has failed, then the result will be a failure object, which will contain the reason.
    ///
    template <std::size_t BufferSize = Request::_traits_::SerializationBufferSizeBytes>
    Expected<ResponsePromise<Response>, Failure> request(const TimePoint                 request_deadline,
                                                         const Request&                  request,
                                                         const cetl::optional<TimePoint> response_deadline = {}) const
    {
        using Result             = Expected<ResponsePromise<Response>, Failure>;
        constexpr bool IsOnStack = BufferSize <= config::Presentation::SmallPayloadSize();

        return detail::tryPerformOnSerialized<Request, Result, BufferSize, IsOnStack>(  //
            request,
            memory(),
            [this, request_deadline, response_deadline](const auto serialized_fragments) -> Result {
                //
                // For request (and the following response) we need to allocate a transfer ID,
                // which will be in use to pair the request with the response.
                //
                auto&      shared_client   = getSharedClient();
                const auto opt_transfer_id = shared_client.nextTransferId();
                if (!opt_transfer_id)
                {
                    return TooManyPendingRequestsError{};
                }
                const auto transfer_id = *opt_transfer_id;

                // Create and register a response promise object, which will be used to handle the response.
                // Its done specifically before sending the request, so that we will be ready to handle a response
                // immediately, even if it happens to be received in context (during) the request sending call.
                //
                ResponsePromise<Response> response_promise{&shared_client,
                                                           transfer_id,
                                                           response_deadline.value_or(request_deadline)};
                //
                const transport::TransferTxMetadata tx_metadata{{transfer_id, getPriority()}, request_deadline};
                if (auto failure = shared_client.sendRequestPayload(tx_metadata, serialized_fragments))
                {
                    return libcyphal::detail::upcastVariant<Failure>(std::move(*failure));
                }

                return response_promise;
            });
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
/// Although the client class does not specifically require the Nunavut tool generated service type, it follows
/// patterns of the tool, so it is highly recommended to use DSDL file and the tool to generate the client type.
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
    /// @brief Initiates a raw request to the server, and returns a promise object to handle the response.
    ///
    /// Issuing a new request involves the following steps:
    /// 1. Allocation of the next transfer ID not in use, so that request and response can be paired. Depending on
    ///    the transport layer (UDP, CAN, etc.), this operation might be O(1) complexity (like for UDP transport,
    ///    where range of transfer ids is 2^64 huge, so simple increment is in use to generate next "unique" id),
    ///    OR it could take O(N) complexity in the worst-case (where N is the number of pending requests), like for
    ///    CAN transport, where N is limited by 2^5. Such limited range of CAN transfer ids is the cause of possible
    ///    `TooManyPendingRequestsError` failure to allocate a new not in use id.
    /// 3. Creation and registration of a response promise object,
    ///    which will be used to handle the raw response from server, and deliver it to the user.
    /// 4. Sending the raw request payload to the server, which might fail with a transport layer error.
    ///    If it does fail, then the response promise object will be destroyed, and the user will get the failure.
    ///
    /// @param request_deadline The deadline for the request sending operation. Request will be dropped if not sent
    ///                         before this deadline, which will inevitably timeout the response waiting deadline.
    /// @param request_payload The raw request payload object to be sent to the server.
    /// @param response_deadline The deadline for the response receiving operation. If `nullopt` (or `{}`) then
    ///                          `request_deadline` will be used for both request & response deadlines.
    /// @return If request sending has succeeded then result will be a promise object to handle the response,
    ///         which will be filled in the future with a received response. See `ResponsePromise` for details.
    ///         If request sending has failed then result will be a failure object, which will contain the reason.
    ///
    Expected<ResponsePromise<void>, Failure> request(const TimePoint                    request_deadline,
                                                     const transport::PayloadFragments& request_payload,
                                                     const cetl::optional<TimePoint>    response_deadline = {}) const
    {
        // 1. For request (and following response) we need to allocate a transfer ID,
        //    which will be in use to pair the request with the response.
        //
        auto&      shared_client   = getSharedClient();
        const auto opt_transfer_id = shared_client.nextTransferId();
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
