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
///       custom behavior defined by the user on a broadcast message, or service request.
class Listener
{
public:
    /// Allows a listener to receive a serialized broadcast
    /// @param[in] rx_metadata the metadata for a message on receive
    /// @param[in] msg The reference to the message to fill in.
    virtual void onReceive(const RxMetadata& rx_metadata, const Message& msg) = 0;

protected:
    ~Listener() = default;
};

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_LISTENER_HPP_INCLUDED
