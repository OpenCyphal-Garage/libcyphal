/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Defines the Transport interface for Transport Layer implementations

#ifndef LIBCYPHAL_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_HPP_INCLUDED

#include <cstdint>
#include "libcyphal/transport/id_types.hpp"
#include "libcyphal/transport/listener.hpp"
#include "libcyphal/transport/message.hpp"
#include "libcyphal/types/status.hpp"

namespace libcyphal
{

using TransportID = uint8_t;

/// The high level interface of a Cyphal Transport.
/// The transport will handle already serialized data.
/// Each transport is assumed to handle all traffic on a specific network segment.
class Transport
{
protected:
    ~Transport() = default;

    /// @brief Initializes and verifies all input variables.
    /// @note Must be run before using Transport
    /// @return Status of whether Transport is ready to use or not
    virtual Status initialize() = 0;

    /// @brief Cleans up queues and connections etc.
    /// @note Must be run to cleanup before Destructor gets called
    /// @return Status of successful cleanup
    virtual Status cleanup() = 0;

    /// @brief Allows a transport to transmit a serialized broadcast
    /// @param[in] tx_metadata the metadata of the message
    /// @param[in] msg The read only reference to the message information.
    /// @retval Success - Message transmitted
    /// @retval Invalid - No record found for response or trying to broadcast anonymously
    /// @retval Failure - Could not transmit the message.
    virtual Status transmit(const transport::TxMetadata& tx_metadata, const Message& msg) = 0;

    /// @brief Called by clients in order to processes incoming UDP Frames
    /// @note The implementation will invoke the listener with the appropriately typed Frames
    /// @param[in] listener The listener to trigger once incoming transfer is processed
    /// @returns The state of the Interface after processing inputs.
    virtual Status processIncomingTransfers(transport::Listener& listener) = 0;

    /// Informs the transport that it will be publishing responses with a specific subject ID. This is to allow
    /// the Transport implementation to prepare for that eventuality if it needs to.
    /// @param[in] port_id The service ID or subject ID of the request/response / broadcast/subscriber pair
    /// @param[in] transfer_kind Transfer kind (message /service request)
    /// @retval result_e::SUCCESS
    /// @retval other values indicate failures
    virtual Status registerPublication(PortID port_id, transport::TransferKind transfer_kind) noexcept = 0;

    /// Registers interest in a specific port ID from this transport.
    /// This allows messages to be delivered to @ref Listener::Receive
    /// @param[in] port_id The PortID to register (subject/service ids)
    /// @param[in] transfer_kind Transfer kind (message /service request)
    /// @retval result_e::SUCCESS
    /// @retval result_e::NOT_ENOUGH, cause_e::RESOURCE Could not take the subscription
    /// @retval result_e::NOT_ALLOWED, cause_e::FINITE_STATE_MACHINE The registration has already been closed
    virtual Status registerSubscription(PortID port_id, transport::TransferKind transfer_kind) noexcept = 0;

    /// @brief Disallow any further subscriptions to be added
    /// @return Whether or not closing the registraiton was successful
    virtual Status closeRegistration() noexcept = 0;
};

/// Used by clients to find the appropriate transports for a given Node ID.
class TransportFinder
{
public:
    ///  Finds the transport associated with a particular route
    virtual Transport* GetTransport(NodeID node_id) = 0;

protected:
    ~TransportFinder() = default;
};

}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_HPP_INCLUDED
