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

#include <linux/can.h>
#include <linux/can/raw.h>

#include "SocketCANInterface.hpp"

namespace libuavcan
{
namespace example
{
SocketCANInterface::SocketCANInterface(std::uint_fast16_t index, int fd)
    : index_(index)
    , fd_(fd)
{}

SocketCANInterface::~SocketCANInterface()
{
    std::cout << "closing socket." << std::endl;
    ::close(fd_);
}

std::uint_fast16_t SocketCANInterface::getInterfaceIndex() const
{
    return index_;
}

libuavcan::Result SocketCANInterface::enqueue(const CanFrame& frame, libuavcan::time::Monotonic tx_deadline)
{
    tx_queue_.emplace(frame, tx_deadline);
    return 0;
}

libuavcan::Result SocketCANInterface::enqueue(const CanFrame& frame)
{
    // Seriously. The difference between 584,942 years and infinity for
    // the tx deadline is ludicrously academic.
    tx_queue_.emplace(frame, libuavcan::time::Monotonic::getMaximum());
    return 0;
}

libuavcan::Result SocketCANInterface::popBack(CanFrame& out_frame)
{
    if (rx_queue_.empty())
    {
        return 0;
    }
    else
    {
        out_frame = rx_queue_.front();
        rx_queue_.pop();
        return 1;
    }
}

libuavcan::Result SocketCANInterface::writeNextFrame()
{
    if (tx_queue_.empty())
    {
        return -1;
    }
    const TxQueueItem& item  = tx_queue_.top();
    const auto&        frame = *item.frame;
    errno                    = 0;

    ::can_frame socketcan_frame;
    // All UAVCAN frames use the extended frame format.
    socketcan_frame.can_id = CAN_EFF_FLAG | (frame.id & CanFrame::MaskExtID);
    socketcan_frame.can_dlc =
        static_cast<std::underlying_type<libuavcan::transport::media::CAN::FrameDLC>::type>(frame.getDLC());
    std::copy(frame.data, frame.data + frame.getDataLength(), socketcan_frame.data);
    // TODO ::canfd_frame CANFD_BRS and CANFD_ESI

    const auto res = ::write(fd_, &socketcan_frame, sizeof(socketcan_frame));
    if (res <= 0)
    {
        if (errno == ENOBUFS || errno == EAGAIN)  // Writing is not possible atm
        {
            return -1;
        }
        return -2;
    }
    if (static_cast<std::size_t>(res) != sizeof(socketcan_frame))
    {
        return -3;
    }
    tx_queue_.pop();
    return 0;
}

libuavcan::Result SocketCANInterface::readOneFrameIntoQueueIfAvailable()
{
    errno = 0;
    ::iovec     iov;
    ::can_frame socketcan_frame;
    // TODO CAN-FD
    iov.iov_base = &socketcan_frame;
    iov.iov_len  = sizeof(socketcan_frame);

    static constexpr size_t ControlSize = sizeof(cmsghdr) + sizeof(::timeval);
    using ControlStorage                = typename std::aligned_storage<ControlSize>::type;
    ControlStorage control_storage;
    std::uint8_t*  control = reinterpret_cast<std::uint8_t*>(&control_storage);
    std::fill(control, control + ControlSize, 0x00);

    ::msghdr msg;
    msg.msg_iov        = &iov;
    msg.msg_iovlen     = 1;
    msg.msg_control    = control;
    msg.msg_controllen = ControlSize;

    const auto res = ::recvmsg(fd_, &msg, MSG_DONTWAIT);
    if (res <= 0)
    {
        return (res < 0 && errno == EWOULDBLOCK) ? 0 : -1;
    }

    std::cout << "got one" << std::endl;
    const ::cmsghdr* const cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMPING)
    {
        auto tv = ::timeval();
        (void) std::memcpy(&tv, CMSG_DATA(cmsg), sizeof(tv));  // Copy to avoid alignment problems.
        auto ts = libuavcan::time::Monotonic::fromMicrosecond(static_cast<std::uint64_t>(tv.tv_sec) * 1000000ULL +
                                                              static_cast<std::uint64_t>(tv.tv_usec));
        LIBUAVCAN_TRACE("SocketCAN",
                        "rx tv_sec=%ld, tv_usec=%ld (ts_utc=%" PRIu64 ")",
                        tv.tv_sec,
                        tv.tv_usec,
                        ts.toMicrosecond());
        rx_queue_.emplace(socketcan_frame.can_id & CAN_EFF_MASK,
                          ts,
                          socketcan_frame.data,
                          libuavcan::transport::media::CAN::FrameDLC(socketcan_frame.can_dlc));
    }
    else
    {
        rx_queue_.emplace(socketcan_frame.can_id & CAN_EFF_MASK,
                          socketcan_frame.data,
                          libuavcan::transport::media::CAN::FrameDLC(socketcan_frame.can_dlc));
    }

    return 0;
}

libuavcan::Result SocketCANInterface::exchange()
{
    writeNextFrame();
    readOneFrameIntoQueueIfAvailable();
    return 0;
}

}  // namespace example
}  // namespace libuavcan

/** @} */  // end of examples group
