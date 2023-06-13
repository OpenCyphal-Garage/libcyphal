/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// The specifier defines the object used to store data about a given session

#ifndef LIBCYPHAL_TRANSPORT_UDP_SPECIFIER_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_SPECIFIER_HPP_INCLUDED

#include "libcyphal/types/status.hpp"
#include "libcyphal/transport/id_types.hpp"
#include "libcyphal/transport/ip/v4/address.hpp"
#include "libcyphal/transport/ip/v4/types.hpp"

namespace libcyphal
{
namespace transport
{
namespace udp
{

/// Holds data that's relevant for Input sessions
struct Specifier
{
    PortID          port_id{UINT16_MAX};
    NodeID          node_id{UDPARD_NODE_ID_UNSET};
    ip::v4::Socket  socket_fd{-1};
    ip::v4::Address target_address{};
    ip::v4::Port    socket_port{UINT16_MAX};
};

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_SPECIFIER_HPP_INCLUDED
