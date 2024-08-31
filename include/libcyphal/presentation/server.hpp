/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_SERVER_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_SERVER_HPP_INCLUDED

#include "server_impl.hpp"

#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/scattered_buffer.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>
#include <cetl/pmr/function.hpp>
#include <nunavut/support/serialization.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
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

/// Trait which determines whether the given type has `T::_traits_::IsService` field.
///
template <typename T>
auto HasIsServiceTrait(bool dummy) -> decltype(T::_traits_::IsService, std::true_type{});
template <typename>
std::false_type HasIsServiceTrait(...);

/// Trait which determines whether the given (supposed to be a Service) type
/// has nested `T::Request` default constructible type.
///
template <typename Service>
auto HasServiceRequest(bool dummy) -> decltype(typename Service::Request{}, std::true_type{});
template <typename>
std::false_type HasServiceRequest(...);

/// Trait which determines whether the given (supposed to be a Service) type
/// has nested `T::Response` default constructible type.
///
template <typename Service>
auto HasServiceResponse(bool dummy) -> decltype(typename Service::Response{}, std::true_type{});
template <typename>
std::false_type HasServiceResponse(...);

/// Trait which determines whether the given type is a service one.
///
/// A service type is expected to have `T::Request` and `T::Response` nested types,
/// as well as `Service::_traits_::IsService` boolean constant equal `true`.
///
template <typename T,
          bool = decltype(HasServiceRequest<T>(true))::value && decltype(HasServiceResponse<T>(true))::value &&
                 decltype(HasIsServiceTrait<T>(true))::value>
struct IsServiceTrait
{
    static constexpr bool value = false;
};
template <typename T>
struct IsServiceTrait<T, true>
{
    static constexpr bool value = T::_traits_::IsService;
};

/// @brief Defines internal base class for any concrete (final) RPC server.
///
/// No Sonar cpp:S4963 "The "Rule-of-Zero" should be followed"
/// b/c we do directly handle resources here.
///
class ServerBase : ServerImpl::Callback  // NOSONAR cpp:S4963
{
public:
    /// @brief Defines failure type for a base server operations.
    ///
    /// The set of possible failures of the base server includes transport layer failures.
    /// A strong-typed server extends this type with its own error types (serialization-related).
    ///
    using Failure = transport::AnyFailure;

    ServerBase(ServerBase&& other) noexcept
        : impl_{std::move(other.impl_)}
    {
        impl_.setOnReceiveCallback(*this);
    }

    ServerBase(const ServerBase& other)                = delete;
    ServerBase& operator=(const ServerBase& other)     = delete;
    ServerBase& operator=(ServerBase&& other) noexcept = delete;

protected:
    /// @brief Defines response continuation functor.
    ///
    /// NB! The functor is supposed to be called only once.
    ///
    template <typename Response, typename SomeFailure>
    class ContinuationImpl final
    {
        constexpr static std::size_t FunctionMaxSize = sizeof(void*) * 5;
        using FunctionSignature = cetl::optional<SomeFailure>(const TimePoint deadline, const Response& response);

    public:
        using Function = cetl::pmr::function<FunctionSignature, FunctionMaxSize>;

        ContinuationImpl() noexcept = default;

        explicit ContinuationImpl(Function&& fn) noexcept
            : fn_{std::move(fn)}
        {
        }

        explicit operator bool() const noexcept
        {
            return static_cast<bool>(fn_);
        }

        /// Sends the response to the client.
        ///
        /// Depending on what was stored inside `fn_`, such sending might involve also serialization.
        /// NB! Supposed to be called only once (will assert if called more than once).
        ///
        /// @param deadline The latest time to send the response. Will be dropped if exceeded.
        /// @param response The response to serialize (optionally), and then send.
        ///
        cetl::optional<SomeFailure> operator()(const TimePoint deadline, const Response& response)
        {
            CETL_DEBUG_ASSERT(fn_, "Continuation function is not set, or called more than once.");

            cetl::optional<SomeFailure> result;
            if (fn_)
            {
                auto func = std::exchange(fn_, nullptr);
                result    = func(deadline, response);
            }
            return result;
        }

    private:
        Function fn_{};

    };  // ContinuationImpl

    explicit ServerBase(ServerImpl&& impl)
        : impl_{std::move(impl)}
    {
        impl_.setOnReceiveCallback(*this);
    }

    ~ServerBase() = default;

    template <typename Request>
    bool tryDeserialize(const transport::ScatteredBuffer& buffer, Request& request)
    {
        return impl_.tryDeserialize(buffer, request);
    }

    cetl::optional<Failure> respondWithPayload(const transport::ServiceTxMetadata& tx_metadata,
                                               const transport::PayloadFragments   payload) const
    {
        return impl_.respondWithPayload(tx_metadata, payload);
    }

private:
    ServerImpl impl_;

};  // ServerBase

}  // namespace detail

/// @brief Defines a custom strong-typed RPC server class.
///
/// Although the server class is not requiring specifically Nunavut tool generated request/response types,
/// it follows patterns of the tool (and has dependency on its `SerializeResult` and `bitspan` helper types),
/// so it is highly recommended to use DSDL file and the tool to generate the types.
/// Otherwise, see below requirements for the `Request` and `Response` types, as well as consult with
/// Nunavut's generated code (f.e. for the signatures of expected `serialize` and `deserialize` functions).
///
/// @tparam Request The request type of the server. This type has the following requirements:
///                 - default constructible
///                 - contains `_traits_::ExtentBytes` constant
///                 - has freestanding `deserialize` function under its namespace (so that ADL will find it).
/// @tparam Response The response type of the server. This type has the following requirements:
///                 - contains `_traits_::SerializationBufferSizeBytes` constant
///                 - has freestanding `serialize` function under its namespace (so that ADL will find it)
///
template <typename Request, typename Response>
class Server final : public detail::ServerBase
{
public:
    /// @brief Defines failure type for a strong-typed server operations.
    ///
    /// The set of possible failures includes transport layer failures (inherited from the base server),
    /// as well as serialization-related ones.
    ///
    using Failure = libcyphal::detail::AppendType<Failure, nunavut::support::Error>::Result;

    /// @brief Defines the strong-typed request callback (arguments, function, and response continuation).
    ///
    struct OnRequestCallback
    {
        struct Arg
        {
            const Request&               request;
            transport::ServiceRxMetadata metadata;
            TimePoint                    approx_now;
        };
        /// Defines continuation functor for sending a strong-typed response.
        using Continuation = ContinuationImpl<Response, Failure>;
        using Function     = cetl::pmr::function<void(const Arg&, Continuation), sizeof(void*) * 4>;
    };

    /// @brief Sets function which will be called on each request reception.
    ///
    /// Note that setting the callback will disable the previous one (if any).
    /// Also, resetting it to `nullptr` does not release internal RX/TX sessions,
    /// and so incoming requests will be still coming and silently dropped (without an attempt to be deserialized).
    ///
    /// @param on_request_cb_fn The function which will be called back.
    ///                         Use `nullptr` (or `{}`) to disable the callback.
    ///
    void setOnRequestCallback(typename OnRequestCallback::Function&& on_request_cb_fn)
    {
        on_request_cb_fn_ = std::move(on_request_cb_fn);
    }

private:
    friend class Presentation;

    explicit Server(detail::ServerImpl&& server_impl)
        : ServerBase{std::move(server_impl)}
    {
    }

    void onRequestRxTransfer(const TimePoint approx_now, const transport::ServiceRxTransfer& rx_transfer) override
    {
        // No need to proceed (deserialization and continuation stuff) if there is no consumer.
        if (!on_request_cb_fn_)
        {
            return;
        }

        // Try to deserialize the strong-typed request from raw bytes.
        // We just drop it if deserialization fails.
        //
        Request request{};
        if (!tryDeserialize(rx_transfer.payload, request))
        {
            return;
        }

        const auto base_metadata  = rx_transfer.metadata.rx_meta.base;
        const auto client_node_id = rx_transfer.metadata.remote_node_id;

        on_request_cb_fn_(  //
            {request, rx_transfer.metadata, approx_now},
            typename OnRequestCallback::Continuation{
                [this, base_metadata, client_node_id](const TimePoint deadline,
                                                      const auto&     response) -> cetl::optional<Failure> {
                    //
                    // Try to serialize the response to raw payload buffer.
                    //
                    // Next nolint b/c we use a buffer to serialize the message, so no need to zero it.
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
                    std::array<std::uint8_t, Response::_traits_::SerializationBufferSizeBytes> buffer;
                    const nunavut::support::bitspan                                            out_bitspan{buffer};
                    const auto result_size = serialize(response, out_bitspan);
                    if (!result_size)
                    {
                        return result_size.error();
                    }

                    // TODO: Eliminate `reinterpret_cast` when Nunavut supports `cetl::byte` at its `serialize`.
                    // Next nolint & NOSONAR are currently unavoidable.
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                    const auto* const data = reinterpret_cast<const cetl::byte*>(buffer.data());  // NOSONAR cpp:S3630
                    const cetl::span<const cetl::byte>                      data_span{data, result_size.value()};
                    const std::array<const cetl::span<const cetl::byte>, 1> payload{data_span};
                    const transport::ServiceTxMetadata tx_metadata{{base_metadata, deadline}, client_node_id};
                    if (auto failure = respondWithPayload(tx_metadata, payload))
                    {
                        return libcyphal::detail::upcastVariant<Failure>(std::move(*failure));
                    }
                    return cetl::nullopt;
                }});
    }

    // MARK: Data members:

    typename OnRequestCallback::Function on_request_cb_fn_;

};  // Server

/// @brief Defines a service typed RPC server class.
///
/// Although the server class is not requiring specifically Nunavut tool generated service type, it follows patterns
/// of the tool, so it is highly recommended to use DSDL file and the tool to generate the service type.
/// Otherwise, see below requirements for the `Service` type, and also `Server<Request, Response>` for details.
///
/// @tparam Service The service type of the server. This type has the following requirements:
///                 - Has `_traits_::IsService` boolean constant equal `true`.
///                 - Has nested `Request` type. See `Server<Request, ...>` for details.
///                 - Has nested `Response` type. See `Server<..., Response>` for details.
///
template <typename Service, typename = std::enable_if_t<detail::IsServiceTrait<Service>::value>>
using ServiceServer = Server<typename Service::Request, typename Service::Response>;

/// @brief Defines a raw (aka untyped) RPC server class.
///
/// The server class has no requirements for the request and response data (neither any Nunavut dependencies).
/// The request/response data is passed as raw bytes (without any serialization/deserialization steps).
///
class RawServiceServer final : public detail::ServerBase
{
public:
    /// @brief Defines a raw untyped request callback (arguments, function, and response continuation).
    ///
    struct OnRequestCallback
    {
        struct Arg
        {
            /// Contains raw bytes of the request payload (aka pre-deserialized).
            const transport::ScatteredBuffer& raw_request;
            transport::ServiceRxMetadata      metadata;
            TimePoint                         approx_now;
        };
        /// Defines continuation functor for sending raw (untyped) response bytes (aka pre-serialized).
        using Continuation = ContinuationImpl<transport::PayloadFragments, Failure>;
        using Function     = cetl::pmr::function<void(const Arg&, Continuation), sizeof(void*) * 4>;
    };

    /// @brief Sets function which will be called on each request reception.
    ///
    /// Note that setting the callback will disable the previous one (if any).
    /// Also, resetting it to `nullptr` does not release internal RX/TX sessions,
    /// and so incoming requests will be still coming and silently dropped.
    ///
    /// @param on_request_cb_fn The function which will be called back.
    ///                         Use `nullptr` (or `{}`) to disable the callback.
    ///
    void setOnRequestCallback(OnRequestCallback::Function&& on_request_cb_fn)
    {
        on_request_cb_fn_ = std::move(on_request_cb_fn);
    }

private:
    friend class Presentation;

    explicit RawServiceServer(detail::ServerImpl&& server_impl)
        : ServerBase{std::move(server_impl)}
    {
    }

    void onRequestRxTransfer(const TimePoint approx_now, const transport::ServiceRxTransfer& rx_transfer) override
    {
        // No need to proceed (deserialization and continuation stuff) if there is no consumer.
        if (!on_request_cb_fn_)
        {
            return;
        }

        const auto base_metadata  = rx_transfer.metadata.rx_meta.base;
        const auto client_node_id = rx_transfer.metadata.remote_node_id;

        on_request_cb_fn_(
            // We pass request payload from transport layer to callback as is (without deserialization).
            {rx_transfer.payload, rx_transfer.metadata, approx_now},
            OnRequestCallback::Continuation{
                [this, base_metadata, client_node_id](const TimePoint                    deadline,
                                                      const transport::PayloadFragments& payload) {
                    //
                    // We pass response payload to transport layer as is (without serialization).
                    const transport::ServiceTxMetadata tx_metadata{{base_metadata, deadline}, client_node_id};
                    return respondWithPayload(tx_metadata, payload);
                }});
    }

    // MARK: Data members:

    OnRequestCallback::Function on_request_cb_fn_;

};  // RawServiceServer

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_SERVER_HPP_INCLUDED
