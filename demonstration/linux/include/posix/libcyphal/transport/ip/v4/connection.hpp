/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Transport IPv4 Connection implementation talking to socket.h

#ifndef POSIX_LIBCYPHAL_TRANSPORT_IP_V4_POSIX_CONNECTION_HPP
#define POSIX_LIBCYPHAL_TRANSPORT_IP_V4_POSIX_CONNECTION_HPP

#ifdef __linux__
#    include <cassert>
#    include <arpa/inet.h>
#    include <sys/socket.h>
#    include <errno.h>
#    include <udpard.h>
#    include <unistd.h>
#    include <cstdint>
#    include <cstring>
#    include <libcyphal/transport/ip/v4/address.hpp>
#    include <libcyphal/transport/ip/v4/types.hpp>
#    include <libcyphal/types/status.hpp>
#    include <libcyphal/media/udp/frame.hpp>
#    include "posix/libcyphal/transport/ip/v4/properties.hpp"
#    include "posix/libcyphal/transport/ip/v4/utils.hpp"

namespace libcyphal
{
namespace transport
{
namespace ip
{
namespace v4
{

/// @brief Creates Posix Socket
/// @return Posix Socket FD
inline Socket createSocket() noexcept
{
    return static_cast<Socket>(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
}

/// @brief Cleans up socket FD
/// @param[in] socket Socket FD
inline Status cleanupSocket(Socket socket_fd)
{
    if (socket_fd != ClosedSocket)
    {
        if (close(socket_fd) == SocketFunctionError)
        {
            return ResultCode::SuccessNothing;
        }
        socket_fd = ClosedSocket;
    }
    assert(socket_fd == ClosedSocket);
    return ResultCode::Success;
}

/// @brief Binds to Posix Socket for a given IPv4 Address and port
/// @param[in] socket_fd  Socket FD
/// @param[in] bind_address IPv4 Address to bind
/// @param[in] linux_socket_port Port to bind
/// @return Status of binding to socket
inline Status bindToSocket(Socket socket_fd, Address bind_address, Port linux_socket_port) noexcept
{
    PosixSocketAddress address = createSocketAddress(bind_address, linux_socket_port);
    if (bind(socket_fd, reinterpret_cast<struct sockaddr*>(&address), static_cast<socklen_t>(sizeof(address))) == SocketFunctionError)
    {
        return ResultCode::FailedToBindToSocket;
    }
    return ResultCode::Success;
}

/// @brief Sends a broadcast message over UDP
/// @param[in] socket_fd Socket FD
/// @param[in] subject_id Subject ID to identify message to send over
/// @param[in] payload Payload buffer to send
/// @param[in] payload_size Size of buffer
/// @return Status of sending broadcast message
inline Status sendBroadcast(Socket              socket_fd,
                            const PortID        subject_id,
                            const std::uint8_t* payload,
                            const std::size_t   payload_size)
{
    // TODO OVPG-3432 Update this function to use the IP address calculated by libudpard instead of recalculating it here
    Address destination_multicast_address = getMulticastAddressFromSubjectId(subject_id);
    PosixSocketAddress remote_addr = createSocketAddress(destination_multicast_address, BroadcastPort);
    if (connect(socket_fd, reinterpret_cast<struct sockaddr*>( &remote_addr ), sizeof(remote_addr)) == SocketFunctionError)
    {
        return ResultCode::Failure;
    }

    if (send(socket_fd, payload, payload_size, 0) == SocketFunctionError)
    {
        return ResultCode::Failure;
    }

    return ResultCode::Success;
}

/// @brief Sends a request or response over UDP
/// @param[in] socket_fd Socket File Descriptor to use for publishing
/// @param[in] remote_node_id The destination Node ID to send the request or response to
/// @param[in] payload Payload buffer to send
/// @param[in] payload_size Size of buffer
/// @return Status of sending the service transfer
inline Status sendServiceTransfer(
    Socket              socket_fd,
    NodeID              remote_node_id,
    const std::uint8_t* payload,
    const std::size_t   payload_size)
{
    // TODO OVPG-3432 Update this function to use the IP address calculated by libudpard instead of recalculating it here
    Address destination_multicast_address = getMulticastAddressFromServiceNodeId(remote_node_id);
    PosixSocketAddress remote_addr = createSocketAddress(destination_multicast_address, BroadcastPort);
    if (connect(socket_fd, reinterpret_cast<struct sockaddr*>(&remote_addr), sizeof(remote_addr)) == SocketFunctionError)
    {
        return ResultCode::Failure;
    }

    if (send(socket_fd, payload, payload_size, 0) == SocketFunctionError)
    {
        return ResultCode::Failure;
    }

    return ResultCode::Success;
}

/// @brief Checks for a message and receives if available
/// @param[in] socket_fd Socket FD
/// @param[in] address IP Address to listen to when receiving message
/// @param[in] socket_port IP Port to listen to when receiving message
/// @param[in,out] buffer_length use as max message payload size, then set as received payload size
/// @param[out] buffer Buffer to store the serialized received message
/// @return Status of receiving message
inline Status receiveMessage(Socket         socket_fd,
                             Address        address,
                             Port           socket_port,
                             media::udp::Frame& out_frame)
{
    PosixSocketAddress socket_address = createSocketAddress(address, socket_port);
    socklen_t socket_size = sizeof(socket_address);

    ssize_t bytes_read = recvfrom(
        socket_fd,
        out_frame.data_,
        out_frame.data_length_,
        MSG_DONTWAIT,
        reinterpret_cast<struct sockaddr*>( &socket_address ),
        &socket_size
    );

    if (bytes_read == SocketFunctionError)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return Status(ResultCode::NotAvailable, CauseCode::Resource);
        }
        else
        {
            return Status(ResultCode::Failure, CauseCode::Resource);
        }
    }
    else
    {
        out_frame.data_length_ = static_cast<unsigned>(bytes_read);
        return ResultCode::Success;
    }
}

}  // namespace v4
}  // namespace ip
}  // namespace transport
}  // namespace libcyphal

#endif  // __linux__
#endif  // POSIX_LIBCYPHAL_TRANSPORT_IP_V4_POSIX_CONNECTION_HPP
