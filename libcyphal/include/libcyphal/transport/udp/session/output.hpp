/// @file
/// Output session for UDP transport.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_TRANSPORT_UDP_SESSION_OUTPUT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_SESSION_OUTPUT_HPP_INCLUDED

#include "libcyphal/libcyphal.hpp"
#include "libcyphal/transport/session.hpp"
#include "libcyphal/network/context.hpp"
#include "libcyphal/transport/udp/ard.h"

namespace libcyphal
{
namespace transport
{
namespace udp
{
namespace session
{

struct RedundantNetworkTxInterface final
{
    static_assert(std::is_nothrow_move_constructible<network::SocketPointer<network::ip::Socket>>::value,
                  "SocketPointer must be nothrow move constructible");
    UdpardTxContext*                            context;
    network::SocketPointer<network::ip::Socket> socket;

    explicit RedundantNetworkTxInterface(UdpardTxContext*                              context,
                                         network::SocketPointer<network::ip::Socket>&& socket) noexcept
        : context(context)
        , socket(std::move(socket))
    {
        CETL_DEBUG_ASSERT(context != nullptr, "Context must not be null");
    }
    ~RedundantNetworkTxInterface() noexcept                                    = default;
    RedundantNetworkTxInterface(const RedundantNetworkTxInterface&)            = delete;
    RedundantNetworkTxInterface& operator=(const RedundantNetworkTxInterface&) = delete;
    RedundantNetworkTxInterface& operator=(RedundantNetworkTxInterface&& rhs)  = delete;
    RedundantNetworkTxInterface(RedundantNetworkTxInterface&& rhs) noexcept
        : context(rhs.context)
        , socket(std::move(rhs.socket))
    {
    }
};

using RedundantNetworkTxInterfaceContainer =
    libcyphal::janky::UnsynchronizedStaticVector<RedundantNetworkTxInterface, maxRedundantInterfaces>;

/// The output session logic is extremely simple because most of the work is handled by the UDP/IP stack of the
/// operating system. Here we just split the transfer into frames, encode the frames, and write them into the socket one
/// by one.
class UDPOutputSession : public IOutputSession, public virtual IRunnable
{
public:
    UDPOutputSession(OutputSessionSpecifier                 specifier,
                     PayloadMetadata                        payload_metadata,
                     RedundantNetworkTxInterfaceContainer&& interfaces) noexcept
        : specifier_(specifier)
        , payload_metadata_(payload_metadata)
        , interfaces_(std::move(interfaces))
    {
    }

    UDPOutputSession(const UDPOutputSession&)            = delete;
    UDPOutputSession& operator=(const UDPOutputSession&) = delete;
    UDPOutputSession(UDPOutputSession&&) noexcept        = default;
    UDPOutputSession& operator=(UDPOutputSession&&)      = delete;

    Status initialize() noexcept
    {
        return ResultCode::Success;
    }

    virtual ~UDPOutputSession()
    {
        close();
    }

    // +-----------------------------------------------------------------------+
    // | IRunnable
    // +-----------------------------------------------------------------------+
    Status runFor(std::chrono::microseconds max_run_duration) noexcept override
    {
        std::size_t interface_failures = 0;

        (void) max_run_duration;  // TODO: setup scheduling strategies for input and output.
        // for now we just send one frame at a time. Instead we should continue
        // to send frames, as long as there are frames to send, for max_run_duration.

        for (RedundantNetworkTxInterface& interface : interfaces_)
        {
            const UdpardTxQueueItem* next_item = udpardTxPeek(&interface.context->queue);
            // TODO: check expiry on the item.
            if (nullptr != next_item)
            {
                if (!interface.socket->send(next_item->frame.payload, next_item->frame.payload_size))
                {
                    ++interface_failures;
                    continue; // don't pop
                }
            }
            UdpardTxQueueItem* popped = udpardTxPop(&interface.context->queue, next_item);
            if (popped == next_item)
            {
                interface.context->getMemoryResource().deallocate(popped, sizeof(UdpardTxQueueItem));
            }  // TODO: what if they don't match?
        }

        if (interface_failures > 0)
        {
            return ResultCode::PartialSuccess;
        }
        else
        {
            return ResultCode::Success;
        }
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
        for (RedundantNetworkTxInterface& tx_interface : interfaces_)
        {
            if (tx_interface.socket)
            {
                tx_interface.socket->close();
            }
        }
    }

    // +-----------------------------------------------------------------------+
    // | IOutputSession implementation
    // +-----------------------------------------------------------------------+
    Status send(const Transfer&           transfer,
                TransferPriority          priority,
                std::chrono::milliseconds monotonic_deadline) override
    {
        UdpardTransferMetadata metadata{toUdpardPriority(priority),
                                        toUdpTransferKind(specifier_),
                                        specifier_.getDataSpecifier().getId(),
                                        specifier_.getRemoteNodeId().value_or(AnonymousNodeID),
                                        0};
        for (RedundantNetworkTxInterface& interface : interfaces_)
        {
            CETL_DEBUG_ASSERT(interface.context != nullptr, "tx memory resource is null");
            CETL_DEBUG_ASSERT(interface.context->queue.capacity >= payload_metadata_.extent_bytes,
                              "tx memory resource maximum size is less than the minimum payload size for a single "
                              "transfer.");
            CETL_DEBUG_ASSERT(interface.context->queue.capacity >= interface.context->queue.mtu_bytes,
                              "tx memory resource capacity is smaller than the MTU?");
            const std::int32_t result = udpardTxPush(&interface.context->queue,
                                                     &interface.context->instance,
                                                     toUdpardMicrosecond(monotonic_deadline),
                                                     &metadata,
                                                     transfer.size_bytes(),
                                                     transfer.data());
            if (result < 0)
            {
                // TODO: partial success
                return fromUdpardResult(result);
            }
        }
        return ResultCode::Success;
    }

private:
    OutputSessionSpecifier               specifier_;
    PayloadMetadata                      payload_metadata_;
    RedundantNetworkTxInterfaceContainer interfaces_;
};

}  // namespace session
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_SESSION_OUTPUT_HPP_INCLUDED
