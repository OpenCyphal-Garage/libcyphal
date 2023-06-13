/// @file
/// Input session for UDP transport.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_TRANSPORT_UDP_SESSION_INPUT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_SESSION_INPUT_HPP_INCLUDED

#include "libcyphal/libcyphal.hpp"
#include "libcyphal/transport/session.hpp"
#include "libcyphal/network/context.hpp"
#include "libcyphal/network/poll.hpp"
#include "libcyphal/transport/udp/ard.h"

#include "cetl/variable_length_array.hpp"

namespace libcyphal
{
namespace transport
{
namespace udp
{
namespace session
{

using RedundantNetworkRxInterfaceContainer =
    libcyphal::janky::UnsynchronizedStaticVector<network::SocketPointer<network::ip::Socket>, maxRedundantInterfaces>;

class UDPInputSession : public IInputSession, public virtual IRunnable
{
public:
    /// Can be overridden after instantiation if needed.
    static constexpr std::chrono::seconds DefaultTransferIdTimeout = std::chrono::seconds(2);

    UDPInputSession(const InputSessionSpecifier&           specifier,
                    const PayloadMetadata&                 payload_metadata,
                    UdpardContext&                         rx_context,
                    RedundantNetworkRxInterfaceContainer&& interfaces,
                    network::PollPointer&&                 interface_poll)
        : specifier_{specifier}
        , payload_metadata_{payload_metadata}
        , rx_context_{rx_context}
        , interfaces_{std::move(interfaces)}
        , interface_poll_{std::move(interface_poll)}
        , socket_event_list_{cetl::pf17::pmr::polymorphic_allocator<UdpardRxTransfer>{&rx_context.getMemoryResource()},
                             interfaces.size()}
        , rx_queue_{cetl::pf17::pmr::polymorphic_allocator<UdpardRxTransfer>{&rx_context.getMemoryResource()}}
        , transfer_id_timeout_{DefaultTransferIdTimeout}
    {
    }

    UDPInputSession(const UDPInputSession&)            = delete;
    UDPInputSession& operator=(const UDPInputSession&) = delete;
    UDPInputSession(UDPInputSession&&)                 = default;
    UDPInputSession& operator=(UDPInputSession&&)      = delete;

    /// Initialize the input session.
    /// @return * ResultCode::Success
    ///         * ResultCode::ResourceClosedError,  (no id)   – The input session was already closed.
    ///         * ResultCode::InvalidArgumentError, (no id)   – The input session was constructed with invalid
    ///                                                         arguments.
    ///         * ResultCode::MemoryError,          (no id)   – The memory resource in the `UdpardContext` did not
    ///                                                         enough memory to initialize this object.
    ///         * (any failure result),             id = 0x52 – The result code details a failure to register a socket
    ///                                                         with the provided network::IPoll instance.
    ///         * ResultCode::InvalidStateError,    id = 0x53 – A socket provided to the input session was not bound.
    ///         * (any failure result),             id = 0x54 – The result code details a failure to add a multicast
    ///                                                         membership for one of the bound sockets provided to the
    ///                                                         session.
    Status initialize()
    {
        // +--[local object initialization]-------------------------------------
        if (closedCount() > 0)
        {
            // TODO: what do we do about partial closure?
            return ResultCode::ResourceClosedError;
        }
        if (interfaces_.size() == 0)
        {
            return ResultCode::InvalidArgumentError;
        }
        socket_event_list_.reserve(interfaces_.size());
        if (socket_event_list_.capacity() < interfaces_.size())
        {
            return ResultCode::MemoryError;
        }

        // +--[Per-interface initialization]------------------------------------
        MutableStatus result{ResultCode::Success, FlagsLayer::Transport, 0x00U, 0x00U};
        for (network::SocketPointer<network::ip::Socket>& ip_socket : interfaces_)
        {
            const Status register_result = interface_poll_->registerSocket(ip_socket.get());
            if (register_result != ResultCode::Success)
            {
                result = MutableStatus{register_result.result, FlagsLayer::Transport, 0x00U, 0x52U};
                break;
            }
            const auto bound_address_perhaps = ip_socket->getBoundAddress();
            if (!bound_address_perhaps.has_value())
            {
                // Input sessions shall be given bound sockets.
                result = MutableStatus{ResultCode::InvalidStateError, FlagsLayer::Transport, 0x00U, 0x53U};
                break;
            }
            const Status r = ip_socket->addMulticastMembership(*bound_address_perhaps);
            if (r != ResultCode::Success)
            {
                result = MutableStatus{r.result, FlagsLayer::Transport, 0x00U, 0x54U};
                break;
            }
        }

        // +--[Rollback on failure]---------------------------------------------
        if (!static_cast<Status>(result))
        {
            // Our unInitialize logic is safe to call on partial initialization and will get us back to the
            // pre-initialization state. Basically this is an un-optimized rollback.
            unInitialize();
        }
        return result;
    }

    /// safe to call before init or after initialization failure (Same logic used by dtor)
    void unInitialize() noexcept
    {
        interface_poll_->clear();
        for (network::SocketPointer<network::ip::Socket>& ip_socket : interfaces_)
        {
            const auto bound_address_perhaps = ip_socket->getBoundAddress();
            if (bound_address_perhaps.has_value())
            {
                ip_socket->removeMulticastMembership(*bound_address_perhaps);
            }
        }
    }

    virtual ~UDPInputSession() noexcept
    {
        unInitialize();
        close();
    }

    // +-----------------------------------------------------------------------+
    // | IRunnable
    // +-----------------------------------------------------------------------+
    Status runFor(std::chrono::microseconds max_run_duration) noexcept override
    {
        // TODO: use execution discipline to control the run duration value passed to poll.
        Status poll_result = interface_poll_->poll(socket_event_list_, max_run_duration);
        if (!poll_result)
        {
            return Status{poll_result.result, 0x55};
        }
        for (network::ISocket* socket : socket_event_list_)
        {
            (void) socket;
            (void) rx_context_;
            network::ip::Socket* ip_socket =
                janky::polymorphic_type_traits::safe_downcast<network::ip::Socket>(*socket);
            CETL_DEBUG_ASSERT(nullptr != ip_socket, "Type mismatch in media layer!");
            // ip_socket->receiveFrom();
            // TODO: receiveFrom then call receive on libudpard.
        }
        socket_event_list_.clear();
        return ResultCode::Success;
    }

    Status cancel() noexcept override
    {
        return ResultCode::NotImplementedError;
    }

    // +-----------------------------------------------------------------------+
    // | ISession implementation
    // +-----------------------------------------------------------------------+
    SessionSpecifier getSpecifier() const noexcept override
    {
        return specifier_;
    }

    PayloadMetadata getPayloadMetadata() const noexcept override
    {
        return payload_metadata_;
    }

    void close() noexcept override
    {
        for (network::SocketPointer<network::ip::Socket>& ip_socket : interfaces_)
        {
            ip_socket->close();
        }
    }

    // +-----------------------------------------------------------------------+
    // | IInputSession
    // +-----------------------------------------------------------------------+

    /// Check for available transfers and return
    Status receive(TransferFrom& out_transfer) override
    {
        (void) out_transfer;

        return ResultCode::NotImplementedError;
    }

    std::chrono::milliseconds getTransferIdTimeout() const noexcept override
    {
        return transfer_id_timeout_;
    }

    Status setTransferIdTimeout(std::chrono::milliseconds value) override
    {
        transfer_id_timeout_ = value;
        return ResultCode::Success;
    }

private:
    std::size_t closedCount() const noexcept
    {
        std::size_t count = 0;
        for (const network::SocketPointer<network::ip::Socket>& ip_socket : interfaces_)
        {
            if (ip_socket->getStatus() == ResultCode::ResourceClosedError)
            {
                ++count;
            }
        }
        return count;
    }

    InputSessionSpecifier                specifier_;
    PayloadMetadata                      payload_metadata_;
    UdpardContext&                       rx_context_;
    RedundantNetworkRxInterfaceContainer interfaces_;
    network::PollPointer                 interface_poll_;
    network::IPoll::SocketEventList      socket_event_list_;
    cetl::VariableLengthArray<UdpardRxTransfer, cetl::pf17::pmr::polymorphic_allocator<UdpardRxTransfer>> rx_queue_;
    std::chrono::milliseconds transfer_id_timeout_;
};

constexpr std::chrono::seconds UDPInputSession::DefaultTransferIdTimeout;

}  // namespace session
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_SESSION_INPUT_HPP_INCLUDED
