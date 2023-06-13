/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Transport Interface used to communicate over an UDP bus

#ifndef LIBCYPHAL_TRANSPORT_UDP_INTERFACE_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_INTERFACE_HPP_INCLUDED

#include "libcyphal/media/udp/frame.hpp"
#include "libcyphal/transport/metadata.hpp"
#include "libcyphal/types/status.hpp"

namespace libcyphal
{
namespace transport
{
namespace udp
{

/// @brief The interface used to communicate over a Cyphal supported UDP Bus.
/// @todo This file is the same for the various transports, but with the Frame type being different.
//        We need to see if it's better to continue to duplicate code throughout or if there is a way around
//        the use of templates to create common contracts such as this.
class Interface
{
public:
    /// @brief An interface used by clients to receive Frame messages from the Interface
    class Receiver
    {
    public:
        /// Called by the Interface when a Frame is available
        /// @param[in] frame Frame containing payload data etc.
        virtual void onReceive(const media::udp::Frame& frame) = 0;

    protected:
        ~Receiver() = default;
    };

    /// @brief Transmits a Cyphal Frame
    /// @param[in] metadata Frame metadata for UDP frame
    /// @param[in] frame Frame containing payload data etc.
    virtual Status transmit(const TxMetadata& metadata, const media::udp::Frame& frame) = 0;

    // Called by clients in order to processes incoming Frames
    /// @param[in] receiver Bus interface for starting the process to receive packets from UDP
    virtual Status processIncomingFrames(Receiver& receiver) = 0;

protected:
    ~Interface() = default;
};

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_INTERFACE_HPP_INCLUDED
