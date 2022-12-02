/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#ifndef LIBCYPHAL_EXAMPLE_SOCKETCANINTERFACE_HPP_INCLUDED
#define LIBCYPHAL_EXAMPLE_SOCKETCANINTERFACE_HPP_INCLUDED

#include <memory>
#include <type_traits>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <sys/socket.h>

#include "libcyphal/libcyphal.hpp"
#include "libcyphal/media/interfaces.hpp"
#include "libcyphal/media/can.hpp"

namespace libcyphal
{
/**
 * @defgroup examples Examples
 *
 * @{
 * @file
 * @namespace example   Namespace containing example applications of libcyphal.
 */
namespace example
{
/**
 * Example of a media::Interface implemented for <a
 * href="https://www.kernel.org/doc/Documentation/networking/can.txt">SocketCAN</a>.
 */
class SocketCANInterface final
{
public:
    struct Statistics
    {
        std::uint64_t rx_total       = 0;
        std::uint64_t rx_dropped     = 0;
        std::uint64_t err_tx_timeout = 0;
        std::uint64_t err_lostarb    = 0;
        std::uint64_t err_crtl       = 0;
        std::uint64_t err_prot       = 0;
        std::uint64_t err_trx        = 0;
        std::uint64_t err_ack        = 0;
        std::uint64_t err_bussoff    = 0;
        std::uint64_t err_buserror   = 0;
        std::uint64_t err_restarted  = 0;
    };

    static constexpr std::size_t TxFramesLen = 4;
    static constexpr std::size_t RxFramesLen = 4;
    static constexpr std::size_t ControlSize = sizeof(cmsghdr) + sizeof(::timeval);
    using FrameType      = libcyphal::media::CAN::Frame<libcyphal::media::CAN::TypeFD::MaxFrameSizeBytes>;
    using ControlStorage = std::aligned_storage<ControlSize>::type;
    using SocketCANFrame = ::canfd_frame;

private:
    const std::uint_fast8_t index_;
    const std::string       name_;
    const int               socket_descriptor_;
    Statistics              stats_;
    SocketCANFrame          trx_socketcan_frames_[RxFramesLen];
    ::iovec                 trx_iovec_[RxFramesLen];
    ControlStorage          trx_control_[RxFramesLen];
    ::mmsghdr               trx_msghdrs_[RxFramesLen];

public:
    // +----------------------------------------------------------------------+
    // | RULE OF SIX
    // +----------------------------------------------------------------------+
    SocketCANInterface(const SocketCANInterface&) = delete;
    SocketCANInterface(SocketCANInterface&&)      = delete;
    SocketCANInterface& operator=(const SocketCANInterface&)   = delete;
    SocketCANInterface& operator&&(const SocketCANInterface&&) = delete;

    SocketCANInterface(std::uint_fast8_t index, const std::string& name, int socket_descriptor);

    ~SocketCANInterface();

    /**
     * Get the name used to bind to the interface.
     */
    const std::string& getInterfaceName() const;

    /**
     * Get the current statistics for this interface.
     */
    void getStatistics(Statistics& out_stats) const;

    /**
     * Get the underlying file descriptor this object encapsulates.
     */
    int getSocketDescriptor() const;

    /**
     * Return the index for this interface. This is a zero based non-sparse range used
     * by methods on the libcyphal::media::InterfaceGroup interface.
     *
     * Note that this is <em>not</em> related to the posix socket interface index.
     *
     * @return This interface's index.
     */
    std::uint_fast8_t getInterfaceIndex() const;

    /**
     * See libcyphal::media::InterfaceGroup::write for documentation.
     */
    libcyphal::Result write(const FrameType (&frame)[TxFramesLen],
                            std::size_t  frames_len,
                            std::size_t& out_frames_written);

    /**
     * See libcyphal::media::InterfaceGroup::read for documentation.
     */
    libcyphal::Result read(FrameType (&out_frames)[RxFramesLen], std::size_t& out_frames_read);
};

}  // namespace example
/** @} */  // end of examples group
}  // namespace libcyphal

#endif  // LIBCYPHAL_EXAMPLE_SOCKETCANINTERFACE_HPP_INCLUDED
