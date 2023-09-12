/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Network Interface implementation used to communicate over a CAN bus

#ifndef LIBCYPHAL_TRANSPORT_CAN_INTERFACE_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_INTERFACE_HPP_INCLUDED

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "libcyphal/media/can/frame.hpp"
#include "libcyphal/transport/can/network_interface.hpp"
#include "libcyphal/transport/can/session/output_session.hpp"
#include "libcyphal/transport/can/session/input_session.hpp"
#include "libcyphal/transport/can/types.hpp"
#include "libcyphal/transport/message.hpp"

namespace libcyphal
{
namespace transport
{
namespace can
{

/// @brief The CAN implementation of the network interface to communicate over CAN
class CANInterface final : public NetworkInterface
{
public:
    CANInterface(session::InputSession& input_session, session::OutputSession& output_session) noexcept
        : input_session_{input_session}
        , output_session_{output_session}
    {
    }

    /// @brief Destructor to cleanup CANInterface before exiting
    ~CANInterface() noexcept = default;

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
    Status transmitFrame(const TxMetadata& metadata, const media::can::extended::Frame& frame) noexcept override
    {
        /// @todo Support non-broadcast transmit (service calls)
        return output_session_.broadcast(metadata.port_id, frame);
    }

    /// @brief Called by clients in order to processes incoming UDP Frames
    /// @param[in] receiver receiver object that triggers to the user when receiving a message
    Status processIncomingFrames(NetworkInterface::Receiver& receiver) noexcept override
    {
        return input_session_.receiveFrames(receiver);
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
    session::InputSession&  input_session_;
    session::OutputSession& output_session_;
};

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_INTERFACE_HPP_INCLUDED
