/// @file
/// Cyphal UDP information required at the Network Layer.
/// While Cyphal attempts to keep the Network layer of any given platform fairly opaque there are a certain set of
/// conventions and constants needed to setup a network layer that is compatible with a given transport. For UDP this
/// involves IP addressing.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_NETWORK_IP_UDP_HPP_INCLUDED
#define LIBCYPHAL_NETWORK_IP_UDP_HPP_INCLUDED

#include "libcyphal/libcyphal.hpp"

namespace libcyphal
{
namespace network
{
namespace ip
{
namespace udp
{

/// All Cyphal traffic uses this port.
/// This is a temporary UDP port. We'll register an official one later.
constexpr std::uint16_t CyphalPort = 9382u;


// clang-format off
/// IPv4 address multicast prefix
constexpr std::uint32_t MulticastPrefix             = 0b11101111000000000000000000000000;
//                                                      87654321876543218765432187654321
static_assert(MulticastPrefix == 0xEF000000, "Multicast prefix is incorrect.");

/// Masks the 16 most significant bits of the multicast group address. To check whether the address is Cyphal/UDP.
constexpr std::uint32_t FixedMaskPrefix             = 0b11111111111111110000000000000000;
//                                                      87654321876543218765432187654321
static_assert(FixedMaskPrefix == 0xFFFF0000, "Fixed mask prefix is incorrect.");

/// Masks the 13 least significant bits of the multicast group address (v4/v6) that represent the subject-ID. (Message)
constexpr std::uint32_t SubjectIdMask               = 0b00000000000000000001111111111111;
//                                                      87654321876543218765432187654321
static_assert(SubjectIdMask == 0x00001FFF, "Subject ID mask is incorrect.");

/// Masks the 9 least significant bits of the multicast group address (v4/v6) that represent the service-ID. (Message)
constexpr std::uint32_t ServiceIdMask               = 0b00000000000000000000000111111111;
//                                                      87654321876543218765432187654321
static_assert(ServiceIdMask == 0x000001FF, "Subject ID mask is incorrect.");

/// Masks the 16 least significant bits of the multicast group address (v4/v6) that represent the destination node-ID.
/// (Service)
constexpr std::uint32_t DestinationNodeIDMask       = 0b00000000000000001111111111111111;
//                                                      87654321876543218765432187654321
static_assert(DestinationNodeIDMask == 0xFFFF, "Destination node ID mask is incorrect.");

/// Service, Not Message: Masks the bit that determines whether the address represents a Message (=0) or Service (=1)
constexpr std::uint32_t ServiceNotMessageBitMask    = 0b00000000000000010000000000000000;
//                                                      87654321876543218765432187654321
static_assert(ServiceNotMessageBitMask == 0x10000, "Service/Not Message bit mask is incorrect.");

/// Cyphal/UDP uses this bit to isolate IP header version 0 traffic
/// (note that the IP header version is not, necessarily, the same as the Cyphal Header version)
/// to the 239.0.0.0/10 scope but we can enable the 239.64.0.0/10 scope in the future.
constexpr std::uint32_t CyphalUDPv4AddressVersion   = 0b00000000010000000000000000000000;
//                                                      87654321876543218765432187654321
static_assert(CyphalUDPv4AddressVersion == 0x400000, "Cyphal/UDP v4 address version mask is incorrect.");

// clang-format on

/// Takes a destination node_id; returns the corresponding IP V4/Cyphal-UDP V0 multicast address (for Service).
/// The resulting address is constructed as follows::
/// ```
///
///                 fixed
///               (15 bits)
///            ______________
///           /              \
///           11101111.00000001.nnnnnnnn.nnnnnnnn
///           \__/      ^     ^ \_______________/
///         (4 bits)  Cyphal SNM     (16 bits)
///           IPv4     UDP           destination node-ID (Service)
///         multicast address
///          prefix  version 0
/// ```
/// @param destinationNodeID
/// @param ipv6Addr
/// @return
constexpr std::uint32_t serviceNodeIDToIPV4CIV0MulticastGroup(NodeID destination_node_id)
{
    CETL_DEBUG_ASSERT(destination_node_id <= DestinationNodeIDMask, "Invalid destination node id passed into method.");
    return (MulticastPrefix | ServiceNotMessageBitMask) | destination_node_id;
}

/// Takes a (Message) data_specifier; returns the corresponding multicast address.
/// For IPv4, the resulting address is constructed as follows::
///
///                 fixed          subject-ID (Service)
///             (15 bits)     res. (15 bits)
///          ______________   | _____________
///         /              \  v/             \
///         11101111.00000000.znnnnnnn.nnnnnnnn
///         \__/      ^     ^
///       (4 bits)  Cyphal SNM
///         IPv4     UDP
///       multicast address
///        prefix  version 0
/// ```
constexpr std::uint32_t messageDataSpecifierToIPV4CIV0MulticastGroup(std::uint16_t subjectId)
{
    // Per [Table 4.5] Subject ID is one less than 65535.
    CETL_DEBUG_ASSERT(subjectId < SubjectIdMask, "Invalid subject Id passed into method.");
    return (MulticastPrefix & ~ServiceNotMessageBitMask) | subjectId;
}

}  // namespace udp
}  // namespace ip
}  // namespace network
}  // namespace libcyphal

#endif  // LIBCYPHAL_NETWORK_IP_UDP_HPP_INCLUDED