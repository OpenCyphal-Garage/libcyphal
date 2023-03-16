/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Commonly used types and definitions for IPv4 Connections

#ifndef LIBCYPHAL_TRANSPORT_IP_V4_TYPES_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_IP_V4_TYPES_HPP_INCLUDED

#include <cstdint>
#include <udpard.h>
#include "libcyphal/transport/id_types.hpp"

namespace libcyphal
{
namespace transport
{
namespace ip
{
namespace v4
{

using Port                                 = std::uint16_t;
using Socket                               = std::int16_t;
constexpr Octet        BroadcastOctet      = 0xEF;  // "239" - 0b0111 per spec
constexpr UdpardNodeID NodeIDUnset         = UDPARD_NODE_ID_UNSET;
constexpr Port         BroadcastPort       = 9382U;  // hardcoded until udpated Cyphal spec
constexpr Port         EphemeralPort       = 0;
constexpr Socket       ClosedSocket        = -1;
constexpr std::int16_t SocketFunctionError = -1;

}  // namespace v4
}  // namespace ip
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_IP_V4_TYPES_HPP_INCLUDED
