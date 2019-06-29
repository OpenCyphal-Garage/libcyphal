/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#ifndef LIBUAVCAN_EXAMPLE_SOCKETCANINTERFACE_HPP_INCLUDED
#define LIBUAVCAN_EXAMPLE_SOCKETCANINTERFACE_HPP_INCLUDED

#include <queue>
#include <memory>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <sys/socket.h>

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/transport/media/interfaces.hpp"
#include "libuavcan/transport/media/can.hpp"

namespace libuavcan
{
/**
 * @defgroup examples Examples
 *
 * @{
 * @file
 * @namespace example   Namespace containing example implementations of libuavcan.
 */
namespace example
{
using CanFrame = libuavcan::transport::media::CAN::Frame<libuavcan::transport::media::CAN::Type2_0::MaxFrameSizeBytes>;
using CanInterface                  = libuavcan::transport::media::Interface<CanFrame, 4, 4>;
using SocketCanFrame                = ::canfd_frame;
static constexpr size_t ControlSize = sizeof(cmsghdr) + sizeof(::timeval);
using ControlStorage                = typename std::aligned_storage<ControlSize>::type;

/**
 * Example of a media::Interface implemented for <a
 * href="https://www.kernel.org/doc/Documentation/networking/can.txt">SocketCAN</a>.
 */
class SocketCANInterface : public CanInterface
{
private:
    const std::uint_fast8_t index_;
    const int               fd_;
    SocketCanFrame          trx_socketcan_frames_[RxFramesLen];
    ::iovec                 trx_iovec_[RxFramesLen];
    ControlStorage          trx_control_[RxFramesLen];
    ::mmsghdr               trx_msghdrs_[RxFramesLen];

public:
    SocketCANInterface(std::uint_fast8_t index, int fd);

    virtual ~SocketCANInterface();

    /**
     * Provide time for this object to read and write messages to and from
     * RX and TX queues.
     */
    void execute();

    // +----------------------------------------------------------------------+
    // | CanInterface
    // +----------------------------------------------------------------------+
    virtual std::uint_fast8_t getInterfaceIndex() const override;

    virtual libuavcan::Result write(const CanFrame (&frame)[TxFramesLen],
                                    std::size_t  frames_len,
                                    std::size_t& out_frames_written) override;

    virtual libuavcan::Result read(CanFrame (&out_frames)[RxFramesLen], std::size_t& out_frames_read) override;

private:
    /**
     * dequeue and send one frame from the tx_queue_ if there are any.
     */
    libuavcan::Result writeNextFrame();

    libuavcan::Result readOneFrameIntoQueueIfAvailable();
};

}  // namespace example
/** @} */  // end of examples group
}  // namespace libuavcan

#endif  // LIBUAVCAN_EXAMPLE_SOCKETCANINTERFACE_HPP_INCLUDED
