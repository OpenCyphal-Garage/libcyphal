/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_SERVER_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_SERVER_HPP_INCLUDED

#include "server_impl.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pmr/function.hpp>
#include <nunavut/support/serialization.hpp>

namespace libcyphal
{
namespace presentation
{

/// Internal implementation details of the Presentation layer.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

/// No Sonar cpp:S4963 "The "Rule-of-Zero" should be followed"
/// b/c we do directly handle resources here.
class ServerBase : ServerImpl::Callback  // NOSONAR cpp:S4963
{
public:
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
    /// Supposed to be called only once.
    ///
    template <typename Response, typename SomeFailure>
    class ContinuationImpl final
    {
        constexpr static size_t FunctionMaxSize = sizeof(void*) * 5;
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

        cetl::optional<SomeFailure> operator()(const TimePoint deadline, const Response& response)
        {
            CETL_DEBUG_ASSERT(fn_, "Continuation function is not set, or called more than once.");

            cetl::optional<SomeFailure> result;
            if (fn_)
            {
                result = fn_(deadline, response);
                fn_    = nullptr;
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

// TODO: docs
template <typename Service>
class Server final : public detail::ServerBase
{
    static_assert(Service::_traits_::IsService, "Only Service types are supported by the Server.");
    static_assert(Service::Request::_traits_::IsRequest, "Has to be a Request type.");
    static_assert(Service::Response::_traits_::IsResponse, "Has to be a Response type.");

public:
    using Request  = typename Service::Request;
    using Response = typename Service::Response;
    using Failure  = libcyphal::detail::AppendType<Failure, nunavut::support::Error>::Result;

    struct OnRequestCallback
    {
        struct Arg
        {
            const Request&               request;
            transport::ServiceRxMetadata metadata;
            TimePoint                    approx_now;
        };
        using Continuation = ContinuationImpl<Response, Failure>;
        using Function     = cetl::pmr::function<void(const Arg&, Continuation), sizeof(void*) * 4>;
    };
    void setOnRequestCallback(typename OnRequestCallback::Function&& on_request_cb_fn)
    {
        on_request_cb_fn_ = std::move(on_request_cb_fn);
    }

private:
    friend class Presentation;
    friend class detail::ServerImpl;

    explicit Server(detail::ServerImpl&& server_impl)
        : ServerBase{std::move(server_impl)}
    {
    }

    void onRequestRxTransfer(const TimePoint approx_now, const transport::ServiceRxTransfer& rx_transfer) override
    {
        if (!on_request_cb_fn_)
        {
            return;
        }

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
                    // Next nolint b/c we use a buffer to serialize the message, so no need to zero it (and performance
                    // better). NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
                    std::array<std::uint8_t, Response::_traits_::SerializationBufferSizeBytes> buffer;
                    const auto data_size = serialize(response, buffer);
                    if (!data_size)
                    {
                        return data_size.error();
                    }

                    // TODO: Eliminate `reinterpret_cast` when Nunavut supports `cetl::byte` at its `serialize`.
                    // Next nolint & NOSONAR are currently unavoidable.
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                    const auto* const data = reinterpret_cast<const cetl::byte*>(buffer.data());  // NOSONAR cpp:S3630
                    const cetl::span<const cetl::byte>                      data_span{data, data_size.value()};
                    const std::array<const cetl::span<const cetl::byte>, 1> payload{data_span};
                    const transport::ServiceTxMetadata tx_metadata{{base_metadata, deadline}, client_node_id};
                    if (auto failure = respondWithPayload(tx_metadata, payload))
                    {
                        return failureFromVariant(std::move(*failure));
                    }
                    return cetl::nullopt;
                }});
    }

    // TODO: move this helper to `libcyphal/types.hpp` (near to `AppendType`)
    template <typename FailureVar>
    CETL_NODISCARD static Failure failureFromVariant(FailureVar&& failure_var)
    {
        return cetl::visit(
            [](auto&& failure) -> Failure {
                //
                return std::forward<decltype(failure)>(failure);
            },
            std::forward<FailureVar>(failure_var));
    }

    // MARK: Data members:

    typename OnRequestCallback::Function on_request_cb_fn_;
};
//
template <>
class Server<void> final : public detail::ServerBase
{
public:
    struct OnRequestCallback
    {
        struct Arg
        {
            const transport::ScatteredBuffer& raw_request;
            transport::ServiceRxMetadata      metadata;
            TimePoint                         approx_now;
        };
        using Continuation = ContinuationImpl<transport::PayloadFragments, Failure>;
        using Function     = cetl::pmr::function<void(const Arg&, Continuation), sizeof(void*) * 4>;
    };
    void setOnRequestCallback(OnRequestCallback::Function&& on_request_cb_fn)
    {
        on_request_cb_fn_ = std::move(on_request_cb_fn);
    }

private:
    friend class Presentation;
    friend class detail::ServerImpl;

    explicit Server(detail::ServerImpl&& server_impl)
        : ServerBase{std::move(server_impl)}
    {
    }

    void onRequestRxTransfer(const TimePoint approx_now, const transport::ServiceRxTransfer& rx_transfer) override
    {
        if (!on_request_cb_fn_)
        {
            return;
        }

        const auto base_metadata  = rx_transfer.metadata.rx_meta.base;
        const auto client_node_id = rx_transfer.metadata.remote_node_id;

        on_request_cb_fn_(  //
            {rx_transfer.payload, rx_transfer.metadata, approx_now},
            OnRequestCallback::Continuation{
                [this, base_metadata, client_node_id](const TimePoint                    deadline,
                                                      const transport::PayloadFragments& payload) {
                    //
                    const transport::ServiceTxMetadata tx_metadata{{base_metadata, deadline}, client_node_id};
                    return respondWithPayload(tx_metadata, payload);
                }});
    }

    // MARK: Data members:

    OnRequestCallback::Function on_request_cb_fn_;

};  // Server

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_SERVER_HPP_INCLUDED
