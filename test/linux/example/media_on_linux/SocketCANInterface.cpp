/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/**
 * @defgroup examples Examples
 *
 * @{
 */
#include <iostream>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <net/if.h>
#include <cstring>
#include <linux/can/error.h>

#include "SocketCANInterface.hpp"

namespace libuavcan
{
namespace example
{
namespace
{
constexpr std::size_t get_message_length(const struct ::can_frame frame)
{
    return frame.can_dlc;
}

constexpr std::size_t get_message_length(const struct ::canfd_frame& frame)
{
    return frame.len;
}

inline void set_message_length(struct ::can_frame& frame, std::uint8_t message_length)
{
    frame.can_dlc = message_length;
}

inline void set_message_length(struct ::canfd_frame& frame, std::uint8_t message_length)
{
    frame.len = message_length;
}

}  // namespace

SocketCANInterface::SocketCANInterface(std::uint_fast8_t index, int fd)
    : index_(index)
    , fd_(fd)
    , stats_()
    , trx_socketcan_frames_()
    , trx_iovec_{{&trx_socketcan_frames_[0], sizeof(trx_socketcan_frames_[0])},
                 {&trx_socketcan_frames_[1], sizeof(trx_socketcan_frames_[1])},
                 {&trx_socketcan_frames_[2], sizeof(trx_socketcan_frames_[2])},
                 {&trx_socketcan_frames_[3], sizeof(trx_socketcan_frames_[3])}}
    , trx_msghdrs_{{{nullptr, 0, &trx_iovec_[0], 1, &trx_control_[0], ControlSize, 0}, 0},
                   {{nullptr, 0, &trx_iovec_[1], 1, &trx_control_[1], ControlSize, 0}, 0},
                   {{nullptr, 0, &trx_iovec_[2], 1, &trx_control_[2], ControlSize, 0}, 0},
                   {{nullptr, 0, &trx_iovec_[3], 1, &trx_control_[3], ControlSize, 0}, 0}}
{
    static_assert(RxFramesLen == 4,
                  "The SocketCANInterface example is hard-coded to 4 rx frames to allow static initialization of the "
                  "internal buffers.");
    static_assert(TxFramesLen == RxFramesLen,
                  "The SocketCANInterface example re-uses the same buffers for send and receive (i.e. not thread-safe) "
                  "so TxFramesLen must be the same as RxFramesLen");
}

SocketCANInterface::~SocketCANInterface()
{
    LIBUAVCAN_TRACE("SocketCANInterface", "closing socket.");
    ::close(fd_);
}

std::uint_fast8_t SocketCANInterface::getInterfaceIndex() const
{
    return index_;
}

void SocketCANInterface::getStatistics(Statistics& out_stats) const
{
    out_stats = stats_;
}

int SocketCANInterface::getFd() const
{
    return fd_;
}

libuavcan::Result SocketCANInterface::write(const FrameType (&frames)[TxFramesLen],
                                            std::size_t  frames_len,
                                            std::size_t& out_frames_written)
{
    errno              = 0;
    out_frames_written = 0;

    if (frames_len == 0)
    {
        return libuavcan::Result::bad_argument;
    }

    for (size_t i = 0; i < frames_len; ++i)
    {
        const FrameType& frame           = frames[i];
        SocketCANFrame&  socketcan_frame = trx_socketcan_frames_[i];

        // All UAVCAN frames use the extended frame format.
        socketcan_frame.can_id = CAN_EFF_FLAG | (frame.id & FrameType::MaskExtID);
        set_message_length(socketcan_frame,
                           static_cast<std::underlying_type<libuavcan::media::CAN::FrameDLC>::type>(frame.getDLC()));
        std::copy(frame.data, frame.data + frame.getDataLength(), socketcan_frame.data);
    }

    const auto res = ::sendmmsg(fd_, trx_msghdrs_, static_cast<unsigned int>(frames_len), 0);

    if (res <= 0)
    {
        if (errno == ENOBUFS || errno == EAGAIN)  // Writing is not possible atm
        {
            return libuavcan::Result::buffer_full;
        }
        return libuavcan::Result::failure;
    }
    out_frames_written = static_cast<std::size_t>(res);
    if (out_frames_written < frames_len)
    {
        return libuavcan::Result::success_partial;
    }
    else
    {
        return libuavcan::Result::success;
    }
}

libuavcan::Result SocketCANInterface::read(FrameType (&out_frames)[RxFramesLen], std::size_t& out_frames_read)
{
    errno           = 0;
    out_frames_read = 0;

    /*
     * We're demonstrating a linux-specific optimization here allowed by the 'n frames' templating of read
     * and write in the interface template. For a posix system that does not support recvmmsg the media
     * layer can simply be defined with RxFramesLen = 1.
     */
    const auto res = ::recvmmsg(fd_, &trx_msghdrs_[0], RxFramesLen, MSG_DONTWAIT, nullptr);

    if (res <= 0)
    {
        return (res < 0 && errno == EWOULDBLOCK) ? libuavcan::Result::success_nothing
                                                 : libuavcan::Result::unknown_internal_error;
    }

    for (std::size_t i = 0; i < static_cast<std::size_t>(res) && i < RxFramesLen; ++i)
    {
        ::msghdr&                  message_header  = trx_msghdrs_[i].msg_hdr;
        const SocketCANFrame&      socketcan_frame = trx_socketcan_frames_[i];
        FrameType&                 out_frame       = out_frames[out_frames_read];
        libuavcan::time::Monotonic timestamp;

        for (::cmsghdr* cmsg = CMSG_FIRSTHDR(&message_header); cmsg; cmsg = CMSG_NXTHDR(&message_header, cmsg))
        {
            if (cmsg->cmsg_level == SOL_SOCKET)
            {
                if (cmsg->cmsg_type == SO_TIMESTAMP && message_header.msg_controllen >= sizeof(::timeval))
                {
                    auto tv = ::timeval();
                    (void) std::memcpy(&tv, CMSG_DATA(cmsg), sizeof(tv));  // Copy to avoid alignment problems.
                    timestamp = libuavcan::time::Monotonic::fromMicrosecond(
                        static_cast<std::uint64_t>(tv.tv_sec) * 1000000ULL + static_cast<std::uint64_t>(tv.tv_usec));
                }
                else if (cmsg->cmsg_type == SO_RXQ_OVFL && cmsg->cmsg_len >= 4)
                {
                    stats_.rx_dropped += *reinterpret_cast<std::uint32_t*>(CMSG_DATA(cmsg));
                }
                else
                {
                    LIBUAVCAN_TRACEF("SocketCANInterface",
                                     "Unknown header found. type=%d, size=%zu",
                                     cmsg->cmsg_type,
                                     message_header.msg_controllen);
                }
            }
            else
            {
                LIBUAVCAN_TRACEF("SocketCANInterface", "Unknown header level. level=%d", cmsg->cmsg_level);
            }
        }

        if (socketcan_frame.can_id & CAN_ERR_FLAG)
        {
            if (socketcan_frame.can_id & CAN_ERR_TX_TIMEOUT)
            {
                stats_.err_tx_timeout += 1;
            }
            if (socketcan_frame.can_id & CAN_ERR_LOSTARB)
            {
                stats_.err_lostarb += 1;
            }
            if (socketcan_frame.can_id & CAN_ERR_CRTL)
            {
                stats_.err_crtl += 1;
            }
            if (socketcan_frame.can_id & CAN_ERR_PROT)
            {
                stats_.err_prot += 1;
            }
            if (socketcan_frame.can_id & CAN_ERR_TRX)
            {
                stats_.err_trx += 1;
            }
            if (socketcan_frame.can_id & CAN_ERR_ACK)
            {
                stats_.err_ack += 1;
            }
            if (socketcan_frame.can_id & CAN_ERR_BUSOFF)
            {
                stats_.err_bussoff += 1;
            }
            if (socketcan_frame.can_id & CAN_ERR_BUSERROR)
            {
                stats_.err_buserror += 1;
            }
            if (socketcan_frame.can_id & CAN_ERR_RESTARTED)
            {
                stats_.err_restarted += 1;
            }
        }
        else if (socketcan_frame.can_id & CAN_EFF_MASK)
        {
            out_frame = {socketcan_frame.can_id,
                         socketcan_frame.data,
                         libuavcan::media::CAN::FrameDLC(get_message_length(socketcan_frame)),
                         timestamp};
            out_frames_read += 1;
            // FUTURE #255: provide frame traceing helpers.
            LIBUAVCAN_TRACEF("SocketCANInterface",
                             "rx [%" PRIu32 ":%" PRIu64 "]",
                             out_frame.id,
                             timestamp.toMicrosecond());
        }
        // else our filters weren't optimal since we shouldn't get non EFF frames that aren't errors.
    }
    stats_.rx_total += static_cast<std::uint32_t>(out_frames_read);
    return libuavcan::Result::success;
}

}  // namespace example
}  // namespace libuavcan

/** @} */  // end of examples group
