/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Defines the Transport interface for Transport Layer implementations

#ifndef LIBCYPHAL_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_HPP_INCLUDED

#include <cstdint>
#include "libcyphal/transport/id_types.hpp"
#include "libcyphal/transport/listener.hpp"
#include "libcyphal/transport/message.hpp"
#include "libcyphal/transport/metadata.hpp"
#include "libcyphal/types/status.hpp"

namespace libcyphal
{

using TransportID = uint8_t;

/// The high level interface of a Cyphal Transport.
/// The transport will handle already serialized data.
/// Each transport is assumed to handle all traffic on a specific network segment.
class Transport
{
public:
    /// @brief Initializes and verifies all input variables.
    /// @note Must be run before using Transport
    /// @return Status of whether Transport is ready to use or not
    virtual Status initialize() = 0;

    /// @brief Cleans up queues and connections etc.
    /// @note Must be run to cleanup before Destructor gets called
    /// @return Status of successful cleanup
    virtual Status cleanup() = 0;

    /// @brief Transmit a serialized message with the subject ID
    /// @param[in] subject_id The subject ID of the message
    /// @param[in] message The read only reference to the payload information.
    /// @retval Success - Message transmitted
    /// @retval Invalid - No record found or trying to broadcast anonymously
    /// @retval Failure - Could not transmit the message.
    virtual Status broadcast(PortID subject_id, const Message& message) = 0;

    /// @brief Transmit a serialized request with the specified service ID
    /// @param[in] service_id The service ID of the request
    /// @param[in] remote_node_id The Node ID to whom the request will be sent
    /// @param[in] request The read only reference to the payload information
    /// @retval Success - Request transmitted
    /// @retval Invalid - No record found for request or trying to publish anonymously
    /// @retval Failure - Could not transmit the request.
    virtual Status sendRequest(PortID service_id, NodeID remote_node_id, const Message& request) = 0;

    /// @brief Transmit a serialized response with the specified service ID
    /// @param[in] service_id The service ID of the response
    /// @param[in] remote_node_id The Node ID to whom the response will be sent
    /// @param[in] response The read only reference to the payload information.
    /// @retval Success - Response transmitted
    /// @retval Invalid - No record found for response or trying to publish anonymously
    /// @retval Failure - Could not transmit the response.
    virtual Status sendResponse(PortID service_id, NodeID remote_node_id, const Message& response) = 0;

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

protected:
    ~Transport() = default;
};

}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_HPP_INCLUDED
