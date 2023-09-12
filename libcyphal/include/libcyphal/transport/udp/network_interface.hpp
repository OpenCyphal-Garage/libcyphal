/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Network Interface used to communicate over an Ethernet bus

#ifndef LIBCYPHAL_TRANSPORT_UDP_NETWORK_INTERFACE_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_NETWORK_INTERFACE_HPP_INCLUDED

#include "libcyphal/media/udp/frame.hpp"
#include "libcyphal/transport/metadata.hpp"
#include "libcyphal/types/status.hpp"

namespace libcyphal
{
namespace transport
{
namespace udp
{

/// @brief The network interface used to communicate over an Ethernet bus.
/// @todo This file is the same for the various transports, but with the Frame type being different.
//        We need to see if it's better to continue to duplicate code throughout or if there is a way around
//        the use of templates to create common contracts such as this.
class NetworkInterface
{
public:
    /// @brief An interface used by clients to receive frames from the bus
    class Receiver
    {
    public:
        /// Called by the Transport when a Frame is available
        /// @param[in] frame Frame containing payload data etc.
        virtual void onReceiveFrame(const media::udp::Frame& frame) = 0;

    protected:
        ~Receiver() = default;
    };

    /// @brief Initialize input session handler
    virtual Status initializeInput() = 0;

    /// @brief Initialize output session handler
    virtual Status initializeOutput() = 0;

    /// @brief Sets up receiver on a given subject ID for subscriber
    /// @param[in] subject_id Subject ID to setup
    virtual Status setupMessageReceiver(const PortID subject_id) = 0;

    /// @brief Sets up receiver to receive requests or responses
    /// @param[in] node_id The local Node ID of the Client or Server
    virtual Status setupServiceReceiver(const NodeID node_id) = 0;

    /// @brief Transmits a Cyphal Frame
    /// @param[in] metadata Frame metadata for UDP frame
    /// @param[in] frame Frame containing payload data etc.
    virtual Status transmitFrame(const TxMetadata& metadata, const media::udp::Frame& frame) = 0;

    // Called by clients in order to processes incoming Frames
    /// @param[in] receiver Bus interface for starting the process to receive packets from UDP
    virtual Status processIncomingFrames(Receiver& receiver) = 0;

protected:
    ~NetworkInterface() = default;
};

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_NETWORK_INTERFACE_HPP_INCLUDED
