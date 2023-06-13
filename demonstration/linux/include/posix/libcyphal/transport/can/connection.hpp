/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// A CAN Connection interface for making CAN connections.
/// Heavily referenced:
/// https://github.com/OpenCyphal-Garage/platform_specific_components/blob/master/socketcan/libcanard/src/socketcan.c

#ifndef POSIX_LIBCYPHAL_TRANSPORT_CAN_CONNECTION_HPP_INCLUDED
#define POSIX_LIBCYPHAL_TRANSPORT_CAN_CONNECTION_HPP_INCLUDED

#ifdef __linux__
#    include <cassert>
#    include <cstdint>
#    include <cstring>
#    include <assert.h>
#    include <fcntl.h>
#    include <linux/can.h>
#    include <linux/can/raw.h>
#    include <errno.h>
#    include <net/if.h>
#    include <poll.h>
#    include <sys/ioctl.h>
#    include <sys/socket.h>
#    include <unistd.h>
#    include <libcyphal/media/can/frame.hpp>
#    include <libcyphal/types/status.hpp>
#    include <libcyphal/types/time.hpp>
#    include <libcyphal/transport/can/types.hpp>
#    include <libcyphal/transport/can/cyphal_can_transport.hpp>

namespace libcyphal
{
namespace transport
{
namespace can
{

constexpr time::Monotonic::MicrosecondType DefaultTransmitTimeoutUs = 0u;  /// Non-Blocking by default
constexpr time::Monotonic::MicrosecondType DefaultReceiveTimeoutUs  = 0u;  /// Non-Blocking by default

/// @brief Create Posix Socket
/// @return Posix Socket FD
inline Socket createSocket() noexcept
{
    return static_cast<Socket>(socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW));
}

/// @brief Cleans up the socket connection
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

/// @brief Binds to a socket connection
/// @param[in] socket_fd POSIX socket file descriptor
/// @param[in] interface_name name of interface to bind to
/// @return Status of binding to socket
inline Status bindToAddress(Socket socket_fd, char* interface_name)
{
    struct ifreq        ifreq;
    struct sockaddr_can address;
    std::memset(&ifreq, 0, sizeof(ifreq));
    std::strncpy(ifreq.ifr_name, interface_name, IFNAMSIZ - 1);
    if (ioctl(socket_fd, SIOCGIFINDEX, &ifreq) == SocketFunctionError)
    {
        return ResultCode::NotFound;
    }
    std::memset(&address, 0, sizeof(address));
    address.can_family  = AF_CAN;
    address.can_ifindex = ifreq.ifr_ifindex;
    if (bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SocketFunctionError)
    {
        return ResultCode::Failure;
    }

    return ResultCode::Success;
}

/// @brief Binds to first available CAN interface
/// @param[in] socket_fd POSIX socket file descriptor
/// @return Status of binding to socket
inline Status bindToAvailableInterface(Socket socket_fd)
{
    /// get a list of interfaces
    struct if_nameindex* name_index = if_nameindex();
    if (name_index == nullptr)
    {
        cleanupSocket(socket_fd);
        return ResultCode::NotAllowed;
    }

    /// iterate through the interfaces and bind the socket to the first available one
    for (struct if_nameindex* nameidx = name_index; nameidx->if_name != nullptr; nameidx++)
    {
        if (bindToAddress(socket_fd, nameidx->if_name).isSuccess())
        {
            break;
        }
    }

    if_freenameindex(name_index);
    return ResultCode::Success;
}

/// @todo Figure out if it's ok to loop through available interfaces
/// @param[in] socket_fd POSIX socket file descriptor
/// @param[in] interface_name The CAN interface name (ex: can0)
inline Status initializeSocket(Socket socket_fd, char* interface_name)
{
    Status result;
    if (socket_fd == ClosedSocket)
    {
        return ResultCode::BadArgument;
    }

    if ((interface_name == nullptr) || (interface_name[0] == '\0'))
    {
        result += bindToAvailableInterface(socket_fd);
    }
    else
    {
        if ((strlen(interface_name) + 1) > IFNAMSIZ)
        {
            return ResultCode::BadArgument;
        }
        result += bindToAddress(socket_fd, const_cast<char*>(interface_name));
    }

    // Enable CAN FD if required.
    const int tmp = 1;
    if (setsockopt(socket_fd, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &tmp, sizeof(tmp)) == SocketFunctionError)
    {
        result += ResultCode::Failure;
    }

    // Enable timestamping.
    if (result.isSuccess())
    {
        const int tmp = 1;
        if (setsockopt(socket_fd, SOL_SOCKET, SO_TIMESTAMP, &tmp, sizeof(tmp)) == SocketFunctionError)
        {
            result += ResultCode::Failure;
        }
    }

    // Enable outgoing-frame loop-back.
    if (result.isSuccess())
    {
        const int tmp = 1;
        if (setsockopt(socket_fd, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &tmp, sizeof(tmp)) == SocketFunctionError)
        {
            result += ResultCode::Failure;
        }
    }

    if (result.isFailure())
    {
        cleanupSocket(socket_fd);
    }
    return result;
}

/// @brief Polls the socket
/// @param[in] socket_fd POSIX socket file descriptor
/// @param[in] mask CAN frame mask
/// @param[in] timeout_usec Timeout in us for the poll
static Status pollSocket(Socket socket_fd, const std::int16_t mask, const time::Monotonic::MicrosecondType timeout_usec)
{
    struct pollfd fds;
    memset(&fds, 0, sizeof(fds));
    fds.fd     = socket_fd;
    fds.events = mask;

    struct timespec ts;
    ts.tv_sec  = static_cast<long>(timeout_usec / static_cast<time::Monotonic::MicrosecondType>(time::Mega));
    ts.tv_nsec = static_cast<long>((timeout_usec % static_cast<time::Monotonic::MicrosecondType>(time::Mega)) *
                 static_cast<time::Monotonic::MicrosecondType>(time::Kilo));

    const int poll_result = ppoll(&fds, 1, &ts, nullptr);
    if (poll_result < 0)
    {
        return ResultCode::SuccessNothing;
    }
    if (poll_result == 0)
    {
        return ResultCode::SuccessTimeout;
    }
    if ((static_cast<std::uint32_t>(fds.revents) & static_cast<std::uint32_t>(mask)) == 0)
    {
        return ResultCode::Failure;
    }

    return ResultCode::Success;
}

/// @brief Transmit CAN frame
/// @param[in] socket_fd POSIX socket file descriptor
/// @param[in] frame the CAN frame to push
/// @param[in] timeout_usec timeout of the polling
static Status push(Socket                                 socket_fd,
                   const media::can::extended::Frame&     frame,
                   const time::Monotonic::MicrosecondType timeout_usec)
{
    const Status poll_result = pollSocket(socket_fd, POLLOUT, timeout_usec);
    if (poll_result.isSuccess())
    {
        struct canfd_frame cfd;
        (void) memset(&cfd, 0, sizeof(cfd));
        cfd.can_id = frame.id_.getID() | CAN_EFF_FLAG;
        cfd.len    = static_cast<std::uint8_t>(frame.dlc_.toLength());
        // We set the bit rate switch on the assumption that it will be ignored by non-CAN-FD-capable hardware.
        cfd.flags = CANFD_BRS;
        (void) memcpy(cfd.data, frame.data_, frame.dlc_.toLength());

        // If the payload is small, use the smaller MTU for compatibility with non-FD sockets.
        // This way, if the user attempts to transmit a CAN FD frame without having the CAN FD socket option
        // enabled, an error will be triggered here.  This is convenient -- we can handle both FD and Classic CAN
        // uniformly.
        const std::size_t mtu = (frame.dlc_.toLength() > CAN_MAX_DLEN) ? CANFD_MTU : CAN_MTU;
        if (write(socket_fd, &cfd, mtu) < 0)
        {
            return ResultCode::Failure;
        }
    }
    return poll_result;
}

/// @brief Receives CAN frame
/// @param[in] socket_fd POSIX socket file descriptor
/// @param[in] timeout_usec timeout to poll for messages coming in
/// @param[out] out_frame output of the frame to populate
static Status pop(Socket                                 socket_fd,
                  media::can::extended::Frame&           out_frame,
                  const time::Monotonic::MicrosecondType timeout_usec)
{
    const Status result = pollSocket(socket_fd, POLLIN, timeout_usec);
    if (result.isSuccess())
    {
        struct canfd_frame sockcan_frame;
        (void)memset(&sockcan_frame, 0, sizeof(sockcan_frame));
        struct iovec       iov
        {};
        iov.iov_base = &sockcan_frame;
        iov.iov_len  = sizeof(sockcan_frame);

        union
        {
            std::uint8_t   buf[CMSG_SPACE(sizeof(struct timeval))];
            struct cmsghdr align;
        } control;
        (void) memset(control.buf, 0, sizeof(control.buf));

        // Initialize the message header used by recvmsg.
        struct msghdr msg;                         // Message header struct.
        (void)memset(&msg, 0, sizeof(msg));
        msg.msg_iov        = &iov;                 // Scatter/gather array.
        msg.msg_iovlen     = 1;                    // Number of elements in the scatter/gather array.
        msg.msg_control    = control.buf;          // Ancillary data.
        msg.msg_controllen = sizeof(control.buf);  // Ancillary data buffer length.

        // Non-blocking receive messages from the socket and validate.
        const ssize_t read_size = recvmsg(socket_fd, &msg, MSG_DONTWAIT);
        if (read_size < 0)
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
        if ((static_cast<std::size_t>(read_size) != CAN_MTU) && (static_cast<std::size_t>(read_size) != CANFD_MTU))
        {
            return ResultCode::Invalid;
        }

        const bool valid = ((sockcan_frame.can_id & CAN_EFF_FLAG) != 0) &&  // Extended frame
                           ((sockcan_frame.can_id & CAN_ERR_FLAG) == 0) &&  // Not RTR frame
                           ((sockcan_frame.can_id & CAN_RTR_FLAG) == 0);    // Not error frame
        if (!valid)
        {
            return ResultCode::SuccessNothing;  // Not an extended data frame -- drop silently and return early.
        }

        // Obtain the CAN frame time stamp from the kernel.
        // This time stamp is from the CLOCK_REALTIME kernel source.
        const struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        struct timeval        tv;
        (void)memset(&tv, 0, sizeof(tv));
        assert(cmsg != nullptr);
        if ((cmsg->cmsg_level == SOL_SOCKET) && (cmsg->cmsg_type == SO_TIMESTAMP))
        {
            (void) memcpy(&tv, CMSG_DATA(cmsg), sizeof(tv));  // Copy to avoid alignment problems
            assert(tv.tv_sec >= 0 && tv.tv_usec >= 0);
        }
        else
        {
            assert(0);
            return ResultCode::Failure;
        }

        out_frame = media::can::extended::Frame();

        out_frame.received_timestamp_ = static_cast<time::Monotonic::MicrosecondType>(
            (static_cast<std::uint64_t>(tv.tv_sec) * time::Mega) + static_cast<std::uint64_t>(tv.tv_usec));
        out_frame.id_.value = (sockcan_frame.can_id & CAN_EFF_MASK);
        out_frame.dlc_      = media::can::nearestDataLengthCode(sockcan_frame.len);
        memcpy(&(out_frame.data_), &sockcan_frame.data[0], sockcan_frame.len);
    }

    return result;
}

/// @brief Transmits CAN over CAN bus
/// @param[in] socket_fd POSIX socket file descriptor
/// @param[in] frame The CAN frame to send
/// @param[in] timeout_usec Timeout of sending CAN frame
inline Status transmitMessage(Socket                                 socket_fd,
                              const media::can::extended::Frame&     frame,
                              const time::Monotonic::MicrosecondType timeout_usec)
{
    return push(socket_fd, frame, timeout_usec);
}

/// @brief Transmits CAN over CAN bus
/// @param[in] socket_fd POSIX socket file descriptor
/// @param[in] frame The CAN frame to send
inline Status transmitMessage(Socket socket_fd, const media::can::extended::Frame& frame)
{
    return transmitMessage(socket_fd, frame, DefaultTransmitTimeoutUs);
}

/// @brief Transmits CAN over CAN bus
/// @param[in] socket_fd POSIX socket file descriptor
/// @param[out] frame The CAN frame to fill
/// @param[in] timeout_usec Timeout of receiving CAN frame
inline Status receiveMessage(Socket                                  socket_fd,
                             media::can::extended::Frame&            frame,
                             const time::Monotonic::MicrosecondType& timeout_usec)
{
    return pop(socket_fd, frame, timeout_usec);
}

/// @brief Transmits CAN over CAN bus
/// @param[in] socket_fd POSIX socket file descriptor
/// @param[out] frame The CAN frame to fill
inline Status receiveMessage(Socket socket_fd, media::can::extended::Frame& frame)
{
    return receiveMessage(socket_fd, frame, DefaultReceiveTimeoutUs);
}

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // __linux__
#endif  // POSIX_LIBCYPHAL_TRANSPORT_CAN_CONNECTION_HPP_INCLUDED
