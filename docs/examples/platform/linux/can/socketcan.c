/// This software is distributed under the terms of the MIT License.
/// Copyright (c) 2020 OpenCyphal
/// Authors: Pavel Kirienko <pavel.kirienko@zubax.com>, Tom De Rybel <tom.derybel@robocow.be>

// This is needed to enable the necessary declarations in sys/
#ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#endif

#include "socketcan.h"

#ifdef __linux__
#    include <linux/can.h>
#    include <linux/can/raw.h>
#    include <net/if.h>
#    include <sys/ioctl.h>
#    include <sys/socket.h>
#else
#    error "Unsupported OS -- feel free to add support for your OS here. " \
        "Zephyr and NuttX are known to support the SocketCAN API."
#endif

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define KILO 1000L
#define MEGA (KILO * KILO)

static int16_t getNegatedErrno(void)
{
    const int out = -abs(errno);
    if (out < 0)
    {
        if (out >= INT16_MIN)
        {
            return (int16_t) out;
        }
    }
    else
    {
        assert(false);  // Requested an error when errno is zero?
    }
    return INT16_MIN;
}

static int16_t doPoll(const SocketCANFD fd, const int16_t mask, const CanardMicrosecond timeout_usec)
{
    struct pollfd fds;
    memset(&fds, 0, sizeof(fds));
    fds.fd     = fd;
    fds.events = mask;

    struct timespec ts;
    ts.tv_sec  = (long) (timeout_usec / (CanardMicrosecond) MEGA);
    ts.tv_nsec = (long) (timeout_usec % (CanardMicrosecond) MEGA) * KILO;

    const int poll_result = ppoll(&fds, 1, &ts, NULL);
    if (poll_result < 0)
    {
        return getNegatedErrno();
    }
    if (poll_result == 0)
    {
        return 0;
    }
    if (((uint32_t) fds.revents & (uint32_t) mask) == 0)
    {
        return -EIO;
    }

    return 1;
}

SocketCANFD socketcanOpen(const char* const iface_name, const bool can_fd)
{
    const size_t iface_name_size = strlen(iface_name) + 1;
    if (iface_name_size > IFNAMSIZ)
    {
        return -ENAMETOOLONG;
    }

    const int fd = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW);  // NOLINT
    bool      ok = fd >= 0;

    if (ok)
    {
        struct ifreq ifr;
        (void) memset(&ifr, 0, sizeof(ifr));
        (void) memcpy(ifr.ifr_name, iface_name, iface_name_size);
        ok = 0 == ioctl(fd, SIOCGIFINDEX, &ifr);
        if (ok)
        {
            struct sockaddr_can addr;
            (void) memset(&addr, 0, sizeof(addr));
            addr.can_family  = AF_CAN;
            addr.can_ifindex = ifr.ifr_ifindex;
            ok               = 0 == bind(fd, (struct sockaddr*) &addr, sizeof(addr));
        }
    }

    // Enable CAN FD if required.
    if (ok && can_fd)
    {
        const int en = 1;
        ok           = 0 == setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &en, sizeof(en));
    }

    // Enable timestamping.
    if (ok)
    {
        const int en = 1;
        ok           = 0 == setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, &en, sizeof(en));
    }

    // Enable outgoing-frame loop-back.
    if (ok)
    {
        const int en = 1;
        ok           = 0 == setsockopt(fd, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &en, sizeof(en));
    }

    if (ok)
    {
        return fd;
    }

    (void) close(fd);
    return getNegatedErrno();
}

int16_t socketcanPush(const SocketCANFD fd, const CanardFrame* const frame, const CanardMicrosecond timeout_usec)
{
    if ((frame == NULL) || (frame->payload.data == NULL) || (frame->payload.size > UINT8_MAX))
    {
        return -EINVAL;
    }

    const int16_t poll_result = doPoll(fd, POLLOUT, timeout_usec);
    if (poll_result > 0)
    {
        // We use the CAN FD struct regardless of whether the CAN FD socket option is set.
        // Per the user manual, this is acceptable because they are binary compatible.
        struct canfd_frame cfd;
        (void) memset(&cfd, 0, sizeof(cfd));
        cfd.can_id = frame->extended_can_id | CAN_EFF_FLAG;
        cfd.len    = (uint8_t) frame->payload.size;
        // We set the bit rate switch on the assumption that it will be ignored by non-CAN-FD-capable hardware.
        cfd.flags = CANFD_BRS;
        (void) memcpy(cfd.data, frame->payload.data, frame->payload.size);

        // If the payload is small, use the smaller MTU for compatibility with non-FD sockets.
        // This way, if the user attempts to transmit a CAN FD frame without having the CAN FD socket option enabled,
        // an error will be triggered here.  This is convenient -- we can handle both FD and Classic CAN uniformly.
        const size_t mtu = (frame->payload.size > CAN_MAX_DLEN) ? CANFD_MTU : CAN_MTU;
        if (write(fd, &cfd, mtu) < 0)
        {
            return getNegatedErrno();
        }
    }
    return poll_result;
}

int16_t socketcanPop(const SocketCANFD        fd,
                     CanardFrame* const       out_frame,
                     CanardMicrosecond* const out_timestamp_usec,
                     const size_t             payload_buffer_size,
                     void* const              payload_buffer,
                     const CanardMicrosecond  timeout_usec,
                     bool* const              loopback)
{
    if ((out_frame == NULL) || (payload_buffer == NULL))
    {
        return -EINVAL;
    }

    const int16_t poll_result = doPoll(fd, POLLIN, timeout_usec);
    if (poll_result > 0)
    {
        // Initialize the message header scatter/gather array. It is to hold a single CAN FD frame struct.
        // We use the CAN FD struct regardless of whether the CAN FD socket option is set.
        // Per the user manual, this is acceptable because they are binary compatible.
        struct canfd_frame sockcan_frame = {0};  // CAN FD frame storage.
        struct iovec       iov           = {
            // Scatter/gather array items struct.
                            .iov_base = &sockcan_frame,        // Starting address.
                            .iov_len  = sizeof(sockcan_frame)  // Number of bytes to transfer.

        };

        // Determine the size of the ancillary data and zero-initialize the buffer for it.
        // We require space for both the receive message header (implied in CMSG_SPACE) and the time stamp.
        // The ancillary data buffer is wrapped in a union to ensure it is suitably aligned.
        // See the cmsg(3) man page (release 5.08 dated 2020-06-09, or later) for details.
        union
        {
            uint8_t        buf[CMSG_SPACE(sizeof(struct timeval))];
            struct cmsghdr align;
        } control;
        (void) memset(control.buf, 0, sizeof(control.buf));

        // Initialize the message header used by recvmsg.
        struct msghdr msg  = {0};                  // Message header struct.
        msg.msg_iov        = &iov;                 // Scatter/gather array.
        msg.msg_iovlen     = 1;                    // Number of elements in the scatter/gather array.
        msg.msg_control    = control.buf;          // Ancillary data.
        msg.msg_controllen = sizeof(control.buf);  // Ancillary data buffer length.

        // Non-blocking receive messages from the socket and validate.
        const ssize_t read_size = recvmsg(fd, &msg, MSG_DONTWAIT);
        if (read_size < 0)
        {
            return getNegatedErrno();  // Error occurred -- return the negated error code.
        }
        if ((read_size != CAN_MTU) && (read_size != CANFD_MTU))
        {
            return -EIO;
        }
        if (sockcan_frame.len > payload_buffer_size)
        {
            return -EFBIG;
        }

        const bool valid = ((sockcan_frame.can_id & CAN_EFF_FLAG) != 0) &&  // Extended frame
                           ((sockcan_frame.can_id & CAN_ERR_FLAG) == 0) &&  // Not RTR frame
                           ((sockcan_frame.can_id & CAN_RTR_FLAG) == 0);    // Not error frame
        if (!valid)
        {
            return 0;  // Not an extended data frame -- drop silently and return early.
        }

        // Handle the loopback frame logic.
        const bool loopback_frame = ((uint32_t) msg.msg_flags & (uint32_t) MSG_CONFIRM) != 0;
        if (loopback == NULL && loopback_frame)
        {
            return 0;  // The loopback pointer is NULL and this is a loopback frame -- drop silently and return early.
        }
        if (loopback != NULL)
        {
            *loopback = loopback_frame;
        }

        // Obtain the CAN frame time stamp from the kernel.
        // This time stamp is from the CLOCK_REALTIME kernel source.
        if (NULL != out_timestamp_usec)
        {
            const struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
            struct timeval        tv   = {0};
            assert(cmsg != NULL);
            if ((cmsg->cmsg_level == SOL_SOCKET) && (cmsg->cmsg_type == SO_TIMESTAMP))
            {
                (void) memcpy(&tv, CMSG_DATA(cmsg), sizeof(tv));  // Copy to avoid alignment problems
                assert(tv.tv_sec >= 0 && tv.tv_usec >= 0);
            }
            else
            {
                assert(0);
                return -EIO;
            }

            (void) memset(out_frame, 0, sizeof(CanardFrame));
            *out_timestamp_usec = (CanardMicrosecond) (((uint64_t) tv.tv_sec * MEGA) + (uint64_t) tv.tv_usec);
        }
        out_frame->extended_can_id = sockcan_frame.can_id & CAN_EFF_MASK;
        out_frame->payload.size    = sockcan_frame.len;
        out_frame->payload.data    = payload_buffer;
        (void) memcpy(payload_buffer, &sockcan_frame.data[0], sockcan_frame.len);
    }
    return poll_result;
}

int16_t socketcanFilter(const SocketCANFD fd, const size_t num_configs, const CanardFilter* const configs)
{
    if (configs == NULL)
    {
        return -EINVAL;
    }
    if (num_configs > CAN_RAW_FILTER_MAX)
    {
        return -EFBIG;
    }

    struct can_filter cfs[CAN_RAW_FILTER_MAX];
    for (size_t i = 0; i < num_configs; i++)
    {
        cfs[i].can_id   = (configs[i].extended_can_id & CAN_EFF_MASK) | CAN_EFF_FLAG;
        cfs[i].can_mask = (configs[i].extended_mask & CAN_EFF_MASK) | CAN_EFF_FLAG | CAN_RTR_FLAG;
    }

    const int ret =
        setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, cfs, (socklen_t) (sizeof(struct can_filter) * num_configs));

    return (ret < 0) ? getNegatedErrno() : 0;
}
