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
    std::cout << "closing socket." << std::endl;
    ::close(fd_);
}

std::uint_fast8_t SocketCANInterface::getInterfaceIndex() const
{
    return index_;
}

libuavcan::Result SocketCANInterface::write(const CanFrame (&frames)[TxFramesLen],
                                            std::size_t  frames_len,
                                            std::size_t& out_frames_written)
{
    errno = 0;
    out_frames_written = 0;

    if (frames_len == 0)
    {
        return libuavcan::results::bad_argument;
    }

    for (size_t i = 0; i < frames_len; ++i)
    {
        const CanFrame& frame           = frames[i];
        SocketCanFrame& socketcan_frame = trx_socketcan_frames_[i];

        // All UAVCAN frames use the extended frame format.
        socketcan_frame.can_id = CAN_EFF_FLAG | (frame.id & CanFrame::MaskExtID);
        set_message_length(socketcan_frame,
                           static_cast<std::underlying_type<libuavcan::transport::media::CAN::FrameDLC>::type>(
                               frame.getDLC()));
        std::copy(frame.data, frame.data + frame.getDataLength(), socketcan_frame.data);
    }

    const auto res = ::sendmmsg(fd_, trx_msghdrs_, static_cast<unsigned int>(frames_len), 0);

    if (res <= 0)
    {
        if (errno == ENOBUFS || errno == EAGAIN)  // Writing is not possible atm
        {
            return libuavcan::results::buffer_full;
        }
        return libuavcan::results::failure;
    }
    out_frames_written = static_cast<std::size_t>(res);
    if (out_frames_written < frames_len)
    {
        return libuavcan::results::success_partial;
    }
    else
    {
        return libuavcan::results::success;
    }
}

libuavcan::Result SocketCANInterface::read(CanFrame (&out_frames)[RxFramesLen], std::size_t& out_frames_read)
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
        return (res < 0 && errno == EWOULDBLOCK) ? libuavcan::results::success_nothing
                                                 : libuavcan::results::unknown_internal_error;
    }

    out_frames_read = static_cast<std::size_t>(res);

    for (std::size_t i = 0; i < static_cast<std::size_t>(res) && i < RxFramesLen; ++i)
    {
        const ::msghdr&        message_header  = trx_msghdrs_[i].msg_hdr;
        const SocketCanFrame&  socketcan_frame = trx_socketcan_frames_[i];
        const ::cmsghdr* const cmsg            = CMSG_FIRSTHDR(&message_header);
        CanFrame&              out_frame       = out_frames[i];

        if (cmsg && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMPING &&
            message_header.msg_controllen >= ControlSize)
        {
            auto tv = ::timeval();
            (void) std::memcpy(&tv, CMSG_DATA(cmsg), sizeof(tv));  // Copy to avoid alignment problems.
            auto ts = libuavcan::time::Monotonic::fromMicrosecond(static_cast<std::uint64_t>(tv.tv_sec) * 1000000ULL +
                                                                  static_cast<std::uint64_t>(tv.tv_usec));

            out_frame = {socketcan_frame.can_id & CAN_EFF_MASK,
                         socketcan_frame.data,
                         libuavcan::transport::media::CAN::FrameDLC(get_message_length(socketcan_frame)),
                         ts};
            // TODO: provide CAN ID trace helpers.
            LIBUAVCAN_TRACEF("SocketCAN",
                             "rx [%" PRIu32 "] tv_sec=%ld, tv_usec=%ld (ts_utc=%" PRIu64 ")",
                             out_frame.id,
                             tv.tv_sec,
                             tv.tv_usec,
                             ts.toMicrosecond());
        }
        else
        {
            out_frame = {socketcan_frame.can_id & CAN_EFF_MASK,
                         socketcan_frame.data,
                         libuavcan::transport::media::CAN::FrameDLC(get_message_length(socketcan_frame))};
            LIBUAVCAN_TRACEF("SocketCAN", "rx [%" PRIu32 "] (no ts)", out_frame.id);
        }
    }
    return libuavcan::results::success;
}

}  // namespace example
}  // namespace libuavcan

/** @} */  // end of examples group
