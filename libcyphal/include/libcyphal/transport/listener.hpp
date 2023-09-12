/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Implemented by classes which listen to transports for incoming message types

#ifndef LIBCYPHAL_TRANSPORT_LISTENER_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_LISTENER_HPP_INCLUDED

#include "libcyphal/transport/metadata.hpp"
#include "libcyphal/transport/message.hpp"

namespace libcyphal
{
namespace transport
{

/// @brief Implemented by classes which listen to transports for incoming message types
/// @note This class can be used by a user and passed into the CyphalUDPTransport.processIncomingTransfers(...) to have
///       custom behavior defined by the user on a broadcast message, service request, or service response.
class Listener
{
public:
    /// Allows a listener to receive a serialized payload
    /// @param[in] rx_metadata The Cyphal metadata for the payload
    /// @param[in] message The reference to the payload buffer to populate
    virtual void onReceive(const RxMetadata& rx_metadata, const Message& payload) = 0;

protected:
    virtual ~Listener() = default;
};

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_LISTENER_HPP_INCLUDED
