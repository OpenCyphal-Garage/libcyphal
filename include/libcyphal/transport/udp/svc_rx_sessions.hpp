/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_SVC_RX_SESSIONS_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_SVC_RX_SESSIONS_HPP_INCLUDED

#include "delegate.hpp"

#include "libcyphal/errors.hpp"
#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/svc_sessions.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <udpard.h>

#include <chrono>
#include <utility>

namespace libcyphal
{
namespace transport
{
namespace udp
{

/// Internal implementation details of the UDP transport.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

/// @brief A base class to represent a service RX session.
///
template <typename Interface_, typename Params>
class SvcRxSessionBase : public IRxSessionDelegate, public Interface_
{
public:
    SvcRxSessionBase(TransportDelegate& delegate, const Params& params)
        : delegate_{delegate}
        , params_{params}
    {
    }

    SvcRxSessionBase(const SvcRxSessionBase&)                = delete;
    SvcRxSessionBase(SvcRxSessionBase&&) noexcept            = delete;
    SvcRxSessionBase& operator=(const SvcRxSessionBase&)     = delete;
    SvcRxSessionBase& operator=(SvcRxSessionBase&&) noexcept = delete;

protected:
    ~SvcRxSessionBase() = default;

    TransportDelegate& delegate()
    {
        return delegate_;
    }

    // MARK: Interface

    CETL_NODISCARD Params getParams() const noexcept final
    {
        return params_;
    }

    CETL_NODISCARD cetl::optional<ServiceRxTransfer> receive() final
    {
        if (last_rx_transfer_)
        {
            auto transfer = std::move(*last_rx_transfer_);
            last_rx_transfer_.reset();
            return transfer;
        }
        return cetl::nullopt;
    }

    void setOnReceiveCallback(ISvcRxSession::OnReceiveCallback::Function&& function) final
    {
        on_receive_cb_fn_ = std::move(function);
    }

    // MARK: IRxSessionDelegate

    void acceptRxTransfer(UdpardRxTransfer& inout_transfer) final
    {
        const auto transfer_id    = inout_transfer.transfer_id;
        const auto remote_node_id = inout_transfer.source_node_id;
        const auto priority       = static_cast<Priority>(inout_transfer.priority);
        const auto timestamp      = TimePoint{std::chrono::microseconds{inout_transfer.timestamp_usec}};

        TransportDelegate::UdpardMemory udpard_memory{delegate_, inout_transfer};

        const ServiceRxMetadata meta{{{transfer_id, priority}, timestamp}, remote_node_id};
        ServiceRxTransfer       svc_rx_transfer{meta, ScatteredBuffer{std::move(udpard_memory)}};
        if (on_receive_cb_fn_)
        {
            on_receive_cb_fn_(ISvcRxSession::OnReceiveCallback::Arg{svc_rx_transfer});
            return;
        }
        (void) last_rx_transfer_.emplace(std::move(svc_rx_transfer));
    }

    // MARK: Data members:

    TransportDelegate&                         delegate_;
    const Params                               params_;
    cetl::optional<ServiceRxTransfer>          last_rx_transfer_;
    ISvcRxSession::OnReceiveCallback::Function on_receive_cb_fn_;

};  // SvcRxSessionBase

// MARK: -

/// @brief A concrete class to represent a service request RX session (aka server side).
///
class SvcRequestRxSession final : public SvcRxSessionBase<IRequestRxSession, RequestRxParams>
{
    /// @brief Defines private specification for making interface unique ptr.
    ///
    struct Spec : libcyphal::detail::UniquePtrSpec<IRequestRxSession, SvcRequestRxSession>
    {
        // `explicit` here is in use to disable public construction of derived private `Spec` structs.
        // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
        explicit Spec() = default;
    };

public:
    CETL_NODISCARD static Expected<UniquePtr<IRequestRxSession>, AnyFailure> make(  //
        cetl::pmr::memory_resource& memory,
        TransportDelegate&          delegate,
        const RequestRxParams&      params,
        RxSessionTreeNode::Request&)
    {
        if (params.service_id > UDPARD_SERVICE_ID_MAX)
        {
            return ArgumentError{};
        }

        auto session = libcyphal::detail::makeUniquePtr<Spec>(memory, Spec{}, delegate, params);
        if (session == nullptr)
        {
            return MemoryError{};
        }

        return session;
    }

    SvcRequestRxSession(const Spec, TransportDelegate& delegate, const RequestRxParams& params)
        : Base{delegate, params}
        , rpc_port_{}
    {
        delegate.listenForRxRpcPort(rpc_port_, params);

        // No Sonar `cpp:S5356` b/c we integrate here with C libudpard API.
        rpc_port_.user_reference = static_cast<IRxSessionDelegate*>(this);  // NOSONAR cpp:S5356
    }

    SvcRequestRxSession(const SvcRequestRxSession&)                = delete;
    SvcRequestRxSession(SvcRequestRxSession&&) noexcept            = delete;
    SvcRequestRxSession& operator=(const SvcRequestRxSession&)     = delete;
    SvcRequestRxSession& operator=(SvcRequestRxSession&&) noexcept = delete;

    ~SvcRequestRxSession()
    {
        delegate().cancelRxRpcPortFor(rpc_port_, true);  // request
        delegate().onSessionEvent(TransportDelegate::SessionEvent::SvcRequestDestroyed{params_});
    }

    // In use (public) for unit tests only.
    const UdpardRxRPCPort& asRpcPort() const noexcept
    {
        return rpc_port_;
    }

private:
    using Base = SvcRxSessionBase;

    // MARK: IRxSession

    void setTransferIdTimeout(const Duration timeout) override
    {
        const auto timeout_us = std::chrono::duration_cast<std::chrono::microseconds>(timeout);
        if (timeout_us >= Duration::zero())
        {
            rpc_port_.port.transfer_id_timeout_usec = static_cast<UdpardMicrosecond>(timeout_us.count());
        }
    }

    // MARK: Data members:

    UdpardRxRPCPort rpc_port_;

};  // SvcRequestRxSession

// MARK: -

/// @brief A concrete class to represent a service response RX session (aka client side).
///
class SvcResponseRxSession final : public SvcRxSessionBase<IResponseRxSession, ResponseRxParams>
{
    /// @brief Defines private specification for making interface unique ptr.
    ///
    struct Spec : libcyphal::detail::UniquePtrSpec<IResponseRxSession, SvcResponseRxSession>
    {
        // `explicit` here is in use to disable public construction of derived private `Spec` structs.
        // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
        explicit Spec() = default;
    };

public:
    CETL_NODISCARD static Expected<UniquePtr<IResponseRxSession>, AnyFailure> make(  //
        cetl::pmr::memory_resource&  memory,
        TransportDelegate&           delegate,
        const ResponseRxParams&      params,
        RxSessionTreeNode::Response& rx_session_node)
    {
        if (params.service_id > UDPARD_SERVICE_ID_MAX)
        {
            return ArgumentError{};
        }

        auto session = libcyphal::detail::makeUniquePtr<Spec>(memory, Spec{}, delegate, params, rx_session_node);
        if (session == nullptr)
        {
            return MemoryError{};
        }

        return session;
    }

    SvcResponseRxSession(const Spec,
                         TransportDelegate&           delegate,
                         const ResponseRxParams&      params,
                         RxSessionTreeNode::Response& rx_session_node)
        : Base{delegate, params}
    {
        delegate.retainRxRpcPortFor(params);

        rx_session_node.delegate() = this;
    }

    SvcResponseRxSession(const SvcResponseRxSession&)                = delete;
    SvcResponseRxSession(SvcResponseRxSession&&) noexcept            = delete;
    SvcResponseRxSession& operator=(const SvcResponseRxSession&)     = delete;
    SvcResponseRxSession& operator=(SvcResponseRxSession&&) noexcept = delete;

    ~SvcResponseRxSession()
    {
        delegate().releaseRxRpcPortFor(params_);
        delegate().onSessionEvent(TransportDelegate::SessionEvent::SvcResponseDestroyed{params_});
    }

private:
    using Base = SvcRxSessionBase;

    // MARK: IRxSession

    void setTransferIdTimeout(const Duration timeout) override
    {
        const auto timeout_us = std::chrono::duration_cast<std::chrono::microseconds>(timeout);
        if (timeout_us >= Duration::zero())
        {
            if (auto* const rpc_port = delegate_.findRxRpcPortFor(params_))
            {
                rpc_port->port.transfer_id_timeout_usec = static_cast<UdpardMicrosecond>(timeout_us.count());
            }
        }
    }

};  // SvcResponseRxSession

}  // namespace detail
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_SVC_RX_SESSIONS_HPP_INCLUDED
