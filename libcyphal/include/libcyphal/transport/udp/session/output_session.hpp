/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Session manager for broadcasts

#ifndef LIBCYPHAL_TRANSPORT_UDP_SESSION_OUTPUT_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_SESSION_OUTPUT_SESSION_HPP_INCLUDED

#include <cstdint>
#include <cstddef>
#include "libcyphal/transport/id_types.hpp"
#include "libcyphal/transport/ip/v4/address.hpp"
#include "libcyphal/types/status.hpp"

namespace libcyphal
{
namespace transport
{
namespace udp
{
namespace session
{

/// @brief Used to store session information for outgoing transfers
/// @todo This file is the same for the various transports, but with the Subscriber Frame type being different.
//        We need to see if it's better to continue to duplicate code throughout or if there is a way around
//        the use of templates to create common contracts such as this.
class OutputSession
{
public:
    /// @brief Initializes the session object
    /// @return Status of initialization
    virtual Status initialize() = 0;

    /// @brief Sends broadcast for given subject id
    /// @param[in] subject_id Subject ID associated with message
    /// @param[in] frame UDP Message frame containing Payload
    /// @return Status of broadcasting data
    virtual Status broadcast(PortID subject_id, const media::udp::Frame& frame) = 0;

    /// @brief Sends a request or response to the specified destination Node ID
    /// @param[in] remote_node_id The destination Node ID who will receive the service transfer
    /// @param[in] frame UDP Frame containing Payload
    /// @return Status of sending the data
    virtual Status sendServiceTransfer(NodeID remote_node_id, const media::udp::Frame& frame) = 0;

protected:
    /// @brief Trivial destructor
    ~OutputSession() noexcept = default;
};

}  // namespace session
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_SESSION_OUTPUT_SESSION_HPP_INCLUDED
