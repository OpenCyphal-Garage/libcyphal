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
using CANFrame = libuavcan::transport::media::CAN::Frame<libuavcan::transport::media::CAN::TypeFD::MaxFrameSizeBytes>;
using SocketCANFrame                = ::canfd_frame;
static constexpr size_t ControlSize = sizeof(cmsghdr) + sizeof(::timeval);
using ControlStorage                = typename std::aligned_storage<ControlSize>::type;

/**
 * Example of a media::Interface implemented for <a
 * href="https://www.kernel.org/doc/Documentation/networking/can.txt">SocketCAN</a>.
 */
class SocketCANInterface : public libuavcan::transport::media::Interface<CANFrame, 4, 4>
{
public:
    struct Statistics
    {
        std::uint32_t rx_total   = 0;
        std::uint32_t rx_dropped = 0;
        std::uint32_t err_tx_timeout = 0;
        std::uint32_t err_lostarb = 0;
        std::uint32_t err_crtl = 0;
        std::uint32_t err_prot = 0;
        std::uint32_t err_trx = 0;
        std::uint32_t err_ack = 0;
        std::uint32_t err_bussoff = 0;
        std::uint32_t err_buserror = 0;
        std::uint32_t err_restarted = 0;
    };

private:
    const std::uint_fast8_t index_;
    const int               fd_;
    Statistics              stats_;
    SocketCANFrame          trx_socketcan_frames_[RxFramesLen];
    ::iovec                 trx_iovec_[RxFramesLen];
    ControlStorage          trx_control_[RxFramesLen];
    ::mmsghdr               trx_msghdrs_[RxFramesLen];

public:
    SocketCANInterface(std::uint_fast8_t index, int fd);

    virtual ~SocketCANInterface();

    /**
     * Get the current statistics for this interface.
     */
    void getStatistics(Statistics& out_stats) const;

    /**
     * Get the underlying file descriptor this object encapsulates.
     */
    int getFd() const;

    // +----------------------------------------------------------------------+
    // | libuavcan::transport::media::Interface
    // +----------------------------------------------------------------------+
    virtual std::uint_fast8_t getInterfaceIndex() const override;

    virtual libuavcan::Result write(const FrameType (&frame)[TxFramesLen],
                                    std::size_t  frames_len,
                                    std::size_t& out_frames_written) override;

    virtual libuavcan::Result read(FrameType (&out_frames)[RxFramesLen], std::size_t& out_frames_read) override;

};

}  // namespace example
/** @} */  // end of examples group
}  // namespace libuavcan

#endif  // LIBUAVCAN_EXAMPLE_SOCKETCANINTERFACE_HPP_INCLUDED
