/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Transport Interface implementation used to communicate over a CAN bus

#ifndef LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "libcyphal/media/can/frame.hpp"
#include "libcyphal/transport/can/interface.hpp"
#include "libcyphal/transport/can/session/message_publisher.hpp"
#include "libcyphal/transport/can/session/message_subscriber.hpp"
#include "libcyphal/transport/can/types.hpp"
#include "libcyphal/transport/message.hpp"

namespace libcyphal
{
namespace transport
{
namespace can
{

/// @brief The CAN implementation of the transport interface to communicate over CAN
class CANTransport final : public Interface
{
public:
    CANTransport(session::MessageSubscriber& input_session, session::MessagePublisher& output_session) noexcept
        : input_session_{input_session}
        , output_session_{output_session}
    {
    }

    /// @brief Destructor to cleanup CANTransport before exiting
    ~CANTransport() noexcept = default;

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

    /// @brief Transmits a CAN Extended Frame
    /// @param[in] metadata CAN Extended Frame metadata
    /// @param[in] frame CAN Extended Frame containing payload, size, and id info
    Status transmit(const TxMetadata metadata, const media::can::extended::Frame& frame) noexcept override
    {
        /// @todo Support non-broadcast transmit (service calls)
        return output_session_.broadcast(metadata.port_id, frame);
    }

    /// @brief Called by clients in order to processes incoming UDP Frames
    /// @param[in] receiver receiver object that triggers to the user when receiving a message
    Status processIncomingFrames(Interface::Receiver& receiver) noexcept override
    {
        if (input_session_.receive(receiver).isFailure())
        {
            return ResultCode::Failure;
        }
        return ResultCode::Success;
    }

    /// @todo Implement this
    Status configure(const media::can::Filter filters[], std::size_t num_filters) noexcept override
    {
        (void) filters;
        (void) num_filters;
        return ResultCode::Success;
    }

    /// @todo Implement this
    std::size_t getNumberOfFilters() const noexcept override
    {
        return 0;
    }

private:
    session::MessageSubscriber& input_session_;
    session::MessagePublisher&  output_session_;
};

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED
