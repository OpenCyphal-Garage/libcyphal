/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal UDP Network Interface implementation used to communicate over an Ethernet bus

#ifndef LIBCYPHAL_TRANSPORT_UDP_INTERFACE_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_INTERFACE_HPP_INCLUDED

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include "libcyphal/media/udp/frame.hpp"
#include "libcyphal/transport/udp/network_interface.hpp"
#include "libcyphal/transport/udp/session/output_session.hpp"
#include "libcyphal/transport/udp/session/input_session.hpp"
#include "libcyphal/types/status.hpp"

namespace libcyphal
{
namespace transport
{
namespace udp
{

/// @brief The UDP implementation of the network interface to communicate over UDP
class UDPInterface final : public NetworkInterface
{
public:
    UDPInterface(session::InputSession& input_session, session::OutputSession& output_session)
        : input_session_{input_session}
        , output_session_{output_session}
    {
    }

    /// @brief Destructor to cleanup UDPInterface before exiting
    ~UDPInterface() noexcept = default;

    /// @brief Initialize subscriber session handler
    Status initializeInput() override
    {
        return input_session_.initialize();
    }

    /// @brief Initialize broadcaster session handler
    Status initializeOutput() override
    {
        return output_session_.initialize();
    }

    /// @brief Sets up receiver on a given subject ID for subscriber
    /// @param[in] subject_id Subject ID to setup
    Status setupMessageReceiver(const PortID subject_id) override
    {
        return input_session_.setupMessageReceiver(subject_id);
    }

    /// @brief Sets up receiver to receive requests or responses
    /// @param[in] node_id The local Node ID of the Client or Server
    Status setupServiceReceiver(const NodeID node_id) override
    {
        return input_session_.setupServiceReceiver(node_id);
    }

    /// @brief Transmits an UDP Frame
    /// @param[in] metadata UDP Frame metadata
    /// @param[in] frame UDP Frame containing payload, size, and header info
    Status transmitFrame(const TxMetadata& metadata, const media::udp::Frame& frame) override
    {
        if (metadata.kind == TransferKind::TransferKindMessage)
        {
            return output_session_.broadcast(metadata.port_id, frame);
        }
        else if ((metadata.kind == TransferKind::TransferKindRequest) ||
                 (metadata.kind == TransferKind::TransferKindResponse))
        {
            return output_session_.sendServiceTransfer(metadata.remote_node_id, frame);
        }
        else
        {
            // Unsupported Transfer Kind
            return Status(ResultCode::BadArgument, CauseCode::Parameter);
        }
    }

    /// @brief Called by users in order to process incoming UDP Frames
    /// @param[in] receiver Receiver object that triggers to the user when receiving a message
    /// @param[in] transfer_kind The type of transfer to receive frames for
    Status processIncomingFrames(NetworkInterface::Receiver& receiver) override
    {
        return input_session_.receiveFrames(receiver);
    }

private:
    session::InputSession&  input_session_;
    session::OutputSession& output_session_;
};

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_INTERFACE_HPP_INCLUDED
