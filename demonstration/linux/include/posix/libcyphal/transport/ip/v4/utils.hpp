/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Various networking helper functions for POSIX Sockets

#ifndef POSIX_LIBCYPHAL_TRANSPORT_IP_V4_POSIX_UTILS_HPP
#define POSIX_LIBCYPHAL_TRANSPORT_IP_V4_POSIX_UTILS_HPP

#ifdef __linux__

#    include <arpa/inet.h>
#    include <sys/socket.h>
#    include <cstdint>
#    include <libcyphal/transport/id_types.hpp>
#    include <libcyphal/transport/ip/v4/address.hpp>
#    include "posix/libcyphal/transport/ip/v4/connection.hpp"

namespace libcyphal
{
namespace transport
{
namespace ip
{
namespace v4
{

using PosixSocketAddress = struct sockaddr_in;

/// @brief Converts IPv4 string to network address
/// @param[in] address string representation of IPv4 address
/// @return 32-bit integer representation of the IPv4 Address
inline std::uint32_t StringAddressToHostBits(const char* address) noexcept
{
    return ntohl(inet_addr(address));
}

/// @brief Converts IPv4 network address to string
/// @param[in] address 32-bit integer representation of IPv4 address
/// @return string representation of the IPv4 Address
inline char* NetworkBitsToStringAddress(const std::uint32_t address) noexcept
{
    struct sockaddr_in socket_address;
    socket_address.sin_addr.s_addr = ntohl(address);
    return inet_ntoa(socket_address.sin_addr);
}

/// @brief Converts IPv4 Address host/network byte order
/// @param[in] address 32-bit integer representation of IPv4 address
/// @return 32-bit integer host/network bits IPv4 Address
inline std::uint32_t NetworkBitsToHostAddress(const std::uint32_t address) noexcept
{
    return ntohl(address);
}

/// @brief Converts port host/network byte order
/// @param[in] port 16-bit representations of port
/// @return host/network byte order converted port
inline std::uint16_t NetworkBitsToHostPort(const std::uint16_t port) noexcept
{
    return ntohs(port);
}

/// @brief Creates a Posix socket address type from an IPv4 Address type
/// @param[in] address IPv4 Address
/// @param[in] port IPv4 POSIX port number
/// @return Posix sockaddr_in representation of the IPv4 Address
inline PosixSocketAddress createSocketAddress(const Address address, const Port port) noexcept
{
    PosixSocketAddress socket_address;
    memset(&socket_address, 0, sizeof(socket_address));
    socket_address.sin_port        = htons(port);
    socket_address.sin_family      = AF_INET;
    socket_address.sin_addr.s_addr = htonl(address.asInteger());

    return socket_address;
}

/// @brief Generates the Broadcast IP Address given a subject id
/// @details Example: Given subject ID 123, the resulting multicast address would be 239.0.0.123
/// @param[in] subject_id Subject ID to get address from
/// @return IPv4Address object
inline Address getMulticastAddressFromSubjectId(PortID subject_id) noexcept
{
    /// The SNM (Service, Not Message) bit determines whether the address represents a Message (=0) or Service (=1)
    const Octet service_not_message_bit = 0x0;
    return Address(
        MulticastOctet,
        service_not_message_bit,
        static_cast<Octet>((subject_id & ThirdOctetMessageMask) >> 8),
        static_cast<Octet>(subject_id & FinalOctetMask));
}

/// @brief Generates the Multicast IP Address for a given Service Node ID
/// @details Example: Given Service Node ID 123, the resulting multicast address would be 239.1.0.123
/// @param[in] service_node_id Service Node ID to use in the multicast address calculation
/// @return IPv4Address object
inline Address getMulticastAddressFromServiceNodeId(NodeID service_node_id) noexcept
{
    /// The SNM (Service, Not Message) bit determines whether the address represents a Message (=0) or Service (=1)
    const Octet service_not_message_bit = 0x1;
    return Address(
        MulticastOctet,
        service_not_message_bit,
        static_cast<Octet>((service_node_id & ThirdOctetServiceMask) >> 8),
        static_cast<Octet>(service_node_id & FinalOctetMask));
}

}  // namespace v4
}  // namespace ip
}  // namespace transport
}  // namespace libcyphal

#endif  // __linux__
#endif  // POSIX_LIBCYPHAL_TRANSPORT_IP_V4_POSIX_UTILS_HPP
