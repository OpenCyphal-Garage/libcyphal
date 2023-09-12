/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Network Interface used to communicate over a CAN bus

#ifndef LIBCYPHAL_TRANSPORT_CAN_NETWORK_INTERFACE_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_NETWORK_INTERFACE_HPP_INCLUDED

#include <cstdint>
#include <cstddef>
#include "libcyphal/media/can/filter.hpp"
#include "libcyphal/media/can/frame.hpp"
#include "libcyphal/transport/metadata.hpp"
#include "libcyphal/types/status.hpp"

namespace libcyphal
{
namespace transport
{
namespace can
{

/// @brief The interface used to communicate over a Cyphal CAN Bus.
/// @todo This file is the same for the various transports, but with the Frame type being different.
//        We need to see if it's better to continue to duplicate code throughout or if there is a way around
//        the use of templates to create common contracts such as this.
class NetworkInterface
{
public:
    /// @brief An interface used by clients to receive Frame messages from the CAN Bus
    class Receiver
    {
    public:
        /// Called by the Transport when a Frame is available
        /// @param[in] frame Frame containing payload data etc.
        virtual void onReceiveFrame(const media::can::extended::Frame& frame) = 0;

    protected:
        ~Receiver() = default;
    };

    /// @brief Initialize input session handler
    virtual Status initializeInput() = 0;

    /// @brief Initialize output session handler
    virtual Status initializeOutput() = 0;

    /// @brief Transmits a Cyphal Frame
    /// @param[in] metadata Frame metadata.
    /// @param[in] frame Frame containing payload data etc.
    virtual Status transmitFrame(const TxMetadata& metadata, const media::can::extended::Frame& frame) = 0;

    // Called by clients in order to processes incoming Frames
    /// @param[in] receiver Bus interface for starting the process to receive packets from UDP
    virtual Status processIncomingFrames(Receiver& receiver) = 0;

    /// Clean-slate configures from the given set of CAN Frame Filters.
    /// @param[in] filters array of CAN Filters
    /// @param[in] num_filters size of the can filters array
    /// @warning This will clear any existing filters and replace them with the incoming set.
    /// @retval result_e::SUCCESS
    /// @retval Other values indicate underlying failures from the Driver.
    virtual Status configure(const media::can::Filter filters[], std::size_t num_filters) noexcept = 0;

    /// Gets the number of currently installed filters
    /// @return number of installed filters
    virtual std::size_t getNumberOfFilters() const noexcept = 0;

protected:
    ~NetworkInterface() = default;
};

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_NETWORK_INTERFACE_HPP_INCLUDED
