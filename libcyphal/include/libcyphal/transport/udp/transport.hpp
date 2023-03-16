/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Transport Interface implementation used to communicate over a UDP bus

#ifndef LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_HPP_INCLUDED

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include "libcyphal/media/udp/frame.hpp"
#include "libcyphal/transport/udp/interface.hpp"
#include "libcyphal/transport/udp/session/message_publisher.hpp"
#include "libcyphal/transport/udp/session/message_subscriber.hpp"
#include "libcyphal/types/status.hpp"

namespace libcyphal
{
namespace transport
{
namespace udp
{

/// @brief The UDP implementation of the transport interface to communicate over UDP
class UDPTransport final : public Interface
{
public:
    UDPTransport(session::MessageSubscriber& input_session, session::MessagePublisher& output_session)
        : input_session_{input_session}
        , output_session_{output_session}
    {
    }

    /// @brief Destructor to cleanup UDPTransport before exiting
    ~UDPTransport() noexcept = default;

    /// @brief Initialize subscriber session handler
    Status initializeInput()
    {
        return input_session_.initialize();
    }

    /// @brief Initialize broadcaster session handler
    Status initializeOutput()
    {
        return output_session_.initialize();
    }

    /// @brief Sets up receiver on a given subject ID for subscriber
    /// @param[in] subject_id Subject ID to setup
    Status setupReceiver(const PortID subject_id)
    {
        return input_session_.setupReceiver(subject_id);
    }

    /// @brief Transmits an UDP Frame via Broadcast
    /// @param[in] metadata UDP Frame metadata
    /// @param[in] frame UDP Frame containing payload, size, and header info
    Status transmit(const TxMetadata metadata, const media::udp::Frame& frame) override
    {
        /// @todo Support non-broadcast transmit (service calls)
        return output_session_.broadcast(metadata.port_id, frame);
    }

    /// @brief Called by clients in order to processes incoming UDP Frames
    /// @param[in] receiver receiver object that triggers to the user when receiving a message
    Status processIncomingFrames(Interface::Receiver& receiver) override
    {
        return input_session_.receive(receiver);
    }

private:
    session::MessageSubscriber& input_session_;
    session::MessagePublisher&  output_session_;
};

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_HPP_INCLUDED
