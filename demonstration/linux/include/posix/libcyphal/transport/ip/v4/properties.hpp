/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Used to set properties on POSIX Sockets

#ifndef POSIX_LIBCYPHAL_TRANSPORT_IP_V4_POSIX_PROPERTIES_HPP
#define POSIX_LIBCYPHAL_TRANSPORT_IP_V4_POSIX_PROPERTIES_HPP

#ifdef __linux__

#    include <arpa/inet.h>
#    include <cstdint>
#    include "libcyphal/types/status.hpp"
#    include "libcyphal/types/time.hpp"

namespace libcyphal
{
namespace transport
{
namespace ip
{
namespace v4
{

constexpr time::Monotonic::MicrosecondType OneSecondInMicroseconds = 1000000;

/// @brief Wrapper around setsockopt to set properties on the socket
/// @param[in] linux_socket The socket connection to join on
/// @param[in] level specifies the protocol level at which the option resides
/// @param[in] property specifies a single option to set
/// @param[in] value The value of the property to set
/// @param[in] value_size the size of the value of the property to set
/// @return status of setting property
inline Status setProperty(const Socket       linux_socket,
                          const std::int16_t level,
                          const std::int16_t property,
                          const void*        value,
                          const socklen_t    value_size) noexcept
{
    return (setsockopt(linux_socket, level, property, value, value_size) < 0) ? ResultCode::Failure
                                                                              : ResultCode::Success;
}

/// @brief Joins a Multicast group
/// @param[in] linux_socket The socket connection to join on
/// @param[in] multicast_group The IPv4 address of the multicast group to join
/// @param[in] multicast_interface The local IPv4 address (source address)
/// @return status of setting property
inline Status setJoinMulticastGroup(const Socket        linux_socket,
                                    const std::uint32_t multicast_group,
                                    const std::uint32_t multicast_interface) noexcept
{
    Status status{};
    struct ip_mreq group;
    group.imr_multiaddr.s_addr = htonl(multicast_group);
    group.imr_interface.s_addr = htonl(multicast_interface);
    status = setProperty(linux_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<char*>(&group), sizeof(group));
    if(status.isFailure()) {
        status = ResultCode::FailedToJoinMulticastGroup;
    }
    return status;
}

/// @brief Allows re-use of an IPv4 Address / socket connection
/// @param[in] linux_socket The socket connection to join on
/// @return status of setting property
inline Status setReuseAddress(const Socket linux_socket) noexcept
{
    u_int yes = 1;
    return setProperty(linux_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
}

/// @brief Joins a Multicast group
/// @param[in] linux_socket The socket connection to join on
/// @param[in] multicast_group The IPv4 address of the multicast group to join
/// @param[in] multicast_interface The local IPv4 address (source address)
/// @return status of setting property
inline Status setReadTimeout(const Socket linux_socket, const time::Monotonic::MicrosecondType time_in_us) noexcept
{
    struct timeval read_timeout;
    read_timeout.tv_sec  = static_cast<time_t>(time_in_us / OneSecondInMicroseconds);
    read_timeout.tv_usec = static_cast<suseconds_t>(time_in_us % OneSecondInMicroseconds);
    return setProperty(linux_socket, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout));
}

/// @brief Sets connection as a multicast connection
/// @param[in] linux_socket The socket connection to join on
/// @param[in] address The local IPv4 address (source address)
/// @return status of setting property
inline Status setMulticast(const Socket linux_socket, const std::uint32_t address) noexcept
{
    struct in_addr multicast_interface;
    multicast_interface.s_addr = htonl(address);
    return setProperty(linux_socket,
                       IPPROTO_IP,
                       IP_MULTICAST_IF,
                       reinterpret_cast<char*>(&multicast_interface),
                       sizeof(multicast_interface));
}

}  // namespace v4
}  // namespace ip
}  // namespace transport
}  // namespace libcyphal

#endif  // __linux__
#endif  // POSIX_LIBCYPHAL_TRANSPORT_IP_V4_POSIX_PROPERTIES_HPP
