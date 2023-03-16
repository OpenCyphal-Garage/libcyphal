/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Session manager for subscriptions. Use this to interface with OS specific calls and objects

#ifndef LIBCYPHAL_TRANSPORT_UDP_SESSION_MESSAGE_SUBSCRIBER_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_SESSION_MESSAGE_SUBSCRIBER_HPP_INCLUDED

#include <cstdint>
#include "libcyphal/media/udp/frame.hpp"
#include "libcyphal/transport/id_types.hpp"
#include "libcyphal/transport/ip/v4/address.hpp"
#include "libcyphal/transport/udp/interface.hpp"
#include "libcyphal/transport/udp/transport.hpp"
#include "libcyphal/types/status.hpp"

namespace libcyphal
{
namespace transport
{
namespace udp
{
namespace session
{

/// @brief Used to store session information for UDP subscriptions
/// @todo Make this usable for service requests also
/// @todo This file is the same for the various transports, but with the Subscriber Frame type being different.
//        We need to see if it's better to continue to duplicate code throughout or if there is a way around
//        the use of templates to create common contracts such as this.
class MessageSubscriber
{
public:
    /// @brief Initializes the session object
    /// @return Status of initialization
    virtual Status initialize() = 0;

    /// @brief Sets up the parameters needed to receive data based off a subject ID
    /// @param[in] subject_id Subject ID used for listening
    /// @return Status of setting up the receiever data
    virtual Status setupReceiver(const PortID subject_id) noexcept = 0;

    /// @brief Sets up the parameters needed to receive data based off a subject ID
    /// @param[in] receiver transport object to trigger receiving logic
    /// @return Status of receiving data
    virtual Status receive(Interface::Receiver& receiver) = 0;

protected:
    /// @brief Trivial destructor
    ~MessageSubscriber() noexcept = default;
};

}  // namespace session
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_SESSION_MESSAGE_SUBSCRIBER_HPP_INCLUDED
