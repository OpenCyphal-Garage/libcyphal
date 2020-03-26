/*
 * Copyright (C) 2014-2015 Pavel Kirienko <pavel.kirienko@gmail.com>
 *                         Ilia Sheremet <illia.sheremet@gmail.com>
 * Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <queue>
#include <vector>
#include <map>
#include <unordered_set>
#include <memory>
#include <algorithm>

#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/time.h>

#ifdef __VXWORKS__
#include <ioLib.h>
#include <lstLib.h>
#include <canDevLib.h>
#include <socketCAN.h>

#else
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/ioctl.h>
#include <net/if.h>

#endif

#include <uavcan/uavcan.hpp>
#include <uavcan/driver/system_clock.hpp>
#include <uavcan_posix/exception.hpp>


namespace uavcan_posix
{

using can_frame = ::canfd_frame;

/**
 * SocketCan driver class keeps number of each kind of errors occurred since the object was created.
 */
enum class SocketCanError
{
    SocketReadFailure,
    SocketWriteFailure,
    TxTimeout
};

/**
 * Single SocketCAN socket interface.
 *
 * SocketCAN socket adapter maintains TX and RX queues in user space. At any moment socket's buffer contains
 * no more than 'max_frames_in_socket_tx_queue_' TX frames, rest is waiting in the user space TX queue; when the
 * socket produces loopback for the previously sent TX frame the next frame from the user space TX queue will
 * be sent into the socket.
 *
 * This approach allows to properly maintain TX timeouts (http://stackoverflow.com/questions/19633015/).
 * TX timestamping is implemented by means of reading RX timestamps of loopback frames (see "TX timestamping" on
 * linux-can mailing list, http://permalink.gmane.org/gmane.linux.can/5322).
 *
 * Note that if max_frames_in_socket_tx_queue_ is greater than one, frame reordering may occur (depending on the
 * underlying logic).
 *
 * This class is too complex and needs to be refactored later. At least, basic socket IO and configuration
 * should be extracted into a different class.
 *
 * When socket filters are enabled, loopback of tx frames are disabled by default by the autoconfiguration 
 * provided by UAVCAN.
 *
 */
class SocketCanIface : public uavcan::ICanIface
{
    friend class SocketCanIfaceTest;

    static inline can_frame makeSocketCanFrame(const uavcan::CanFrame& uavcan_frame)
    {
        can_frame sockcan_frame = can_frame();
        sockcan_frame.can_id = uavcan_frame.id & uavcan::CanFrame::MaskExtID;
        sockcan_frame.len = uavcan_frame.getDataLength();
#ifndef __VXWORKS__
        // TODO VSDK-1684: define CANFD_BRS for vxworks.
        sockcan_frame.flags = CANFD_BRS;
#endif
        (void)std::copy(uavcan_frame.data, uavcan_frame.data + uavcan_frame.getDataLength(), sockcan_frame.data);
        if (uavcan_frame.isExtended())
        {
            sockcan_frame.can_id |= CAN_EFF_FLAG;
        }
        if (uavcan_frame.isErrorFrame())
        {
            sockcan_frame.can_id |= CAN_ERR_FLAG;
        }
        if (uavcan_frame.isRemoteTransmissionRequest())
        {
            sockcan_frame.can_id |= CAN_RTR_FLAG;
        }
        return sockcan_frame;
    }

    static inline uavcan::CanFrame makeUavcanFrame(const can_frame& sockcan_frame)
    {
        uavcan::CanFrame uavcan_frame(sockcan_frame.can_id & CAN_EFF_MASK, sockcan_frame.data, uavcan::CanFrame::length_to_dlc(sockcan_frame.len));
        if (sockcan_frame.can_id & CAN_EFF_FLAG)
        {
            uavcan_frame.id |= uavcan::CanFrame::FlagEFF;
        }
        if (sockcan_frame.can_id & CAN_ERR_FLAG)
        {
            uavcan_frame.id |= uavcan::CanFrame::FlagERR;
        }
        if (sockcan_frame.can_id & CAN_RTR_FLAG)
        {
            uavcan_frame.id |= uavcan::CanFrame::FlagRTR;
        }
        return uavcan_frame;
    }

    struct TxItem
    {
        uavcan::CanFrame frame;
        uavcan::MonotonicTime deadline;
        uavcan::CanIOFlags flags = 0;
        std::uint64_t order = 0;

        TxItem(const uavcan::CanFrame& arg_frame, uavcan::MonotonicTime arg_deadline,
               uavcan::CanIOFlags arg_flags, std::uint64_t arg_order)
            : frame(arg_frame)
            , deadline(arg_deadline)
            , flags(arg_flags)
            , order(arg_order)
        { }

        bool operator<(const TxItem& rhs) const
        {
            if (frame.priorityLowerThan(rhs.frame))
            {
                return true;
            }
            if (frame.priorityHigherThan(rhs.frame))
            {
                return false;
            }
            return order > rhs.order;
        }
    };

    struct RxItem
    {
        uavcan::CanFrame frame;
        uavcan::MonotonicTime ts_mono;
        uavcan::UtcTime ts_utc;
        uavcan::CanIOFlags flags;

        RxItem()
            : flags(0)
        { }
    };

    const uavcan_posix::ISystemClock& clock_;
    const int fd_;
    bool loopback_filter_configured_ = false;

    const unsigned max_frames_in_socket_tx_queue_;
    unsigned frames_in_socket_tx_queue_ = 0;

    std::uint64_t tx_frame_counter_ = 0;        ///< Increments with every frame pushed into the TX queue

    std::map<SocketCanError, std::uint64_t> errors_;

    std::priority_queue<TxItem> tx_queue_;                          // TODO: Use pool allocator
    std::queue<RxItem> rx_queue_;                                   // TODO: Use pool allocator
    std::unordered_multiset<std::uint32_t> pending_loopback_ids_;   // TODO: Use pool allocator

    std::vector<::can_filter> sw_filters_container_;

    void registerError(SocketCanError e) { errors_[e]++; }

    void incrementNumFramesInSocketTxQueue()
    {
        if (!loopback_filter_configured_)
        {
            UAVCAN_ASSERT(frames_in_socket_tx_queue_ < max_frames_in_socket_tx_queue_);
            frames_in_socket_tx_queue_++;
        }
    }

    void confirmSentFrame()
    {
        if (!loopback_filter_configured_)
        {
            if (frames_in_socket_tx_queue_ > 0)
            {
                frames_in_socket_tx_queue_--;
            }
            else
            {
                UAVCAN_ASSERT(0); // Loopback for a frame that we didn't send.
            }
        }
    }

    bool wasInPendingLoopbackSet(const uavcan::CanFrame& frame)
    {
        if (pending_loopback_ids_.count(frame.id) > 0)
        {
            (void)pending_loopback_ids_.erase(frame.id);
            return true;
        }
        return false;
    }

    int write(const uavcan::CanFrame& frame) const
    {
        errno = 0;

        const can_frame sockcan_frame = makeSocketCanFrame(frame);

        const int res = ::write(fd_, &sockcan_frame, sizeof(sockcan_frame));
        if (res <= 0)
        {
            if (errno == ENOBUFS || errno == EAGAIN)    // Writing is not possible atm, not an error
            {
                return 0;
            }
            return res;
        }
        if (res != sizeof(sockcan_frame))
        {
            return -1;
        }
        return 1;
    }

    /**
     * SocketCAN git show 1e55659ce6ddb5247cee0b1f720d77a799902b85
     *    MSG_DONTROUTE is set for any packet from localhost,
     *    MSG_CONFIRM is set for any packet of your socket.
     * Diff: https://git.ucsd.edu/abuss/linux/commit/1e55659ce6ddb5247cee0b1f720d77a799902b85
     * Man: https://www.kernel.org/doc/Documentation/networking/can.txt (chapter 4.1.6).
     */
    int read(uavcan::CanFrame& frame, uavcan::UtcTime& ts_utc, bool& loopback) const
    {
        auto iov = ::iovec();
        auto sockcan_frame = can_frame();
        iov.iov_base = &sockcan_frame;
        iov.iov_len  = sizeof(sockcan_frame);

        static constexpr size_t ControlSize = sizeof(cmsghdr) + sizeof(::timeval);
        using ControlStorage = typename std::aligned_storage<ControlSize>::type;
        ControlStorage control_storage;
        auto control = reinterpret_cast<std::uint8_t *>(&control_storage);
        std::fill(control, control + ControlSize, 0x00);

        auto msg = ::msghdr();
        msg.msg_iov    = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = control;
        msg.msg_controllen = ControlSize;

        const int res = ::recvmsg(fd_, &msg, MSG_DONTWAIT);
        if (res <= 0)
        {
            return (res < 0 && errno == EWOULDBLOCK) ? 0 : res;
        }
        /*
         * Flags
         */
        loopback = (msg.msg_flags & static_cast<int>(MSG_CONFIRM)) != 0;

        if (!loopback && !checkSWFilters(sockcan_frame))
        {
            return 0;
        }

        frame = makeUavcanFrame(sockcan_frame);
        /*
         * Timestamp
         */
#ifndef __VXWORKS__
        const ::cmsghdr* const cmsg = CMSG_FIRSTHDR(&msg);
        UAVCAN_ASSERT(cmsg != nullptr);
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMP)
        {
            auto tv = ::timeval();
            (void)std::memcpy(&tv, CMSG_DATA(cmsg), sizeof(tv));  // Copy to avoid alignment problems
            UAVCAN_ASSERT(tv.tv_sec >= 0 && tv.tv_usec >= 0);
            ts_utc = uavcan::UtcTime::fromUSec(std::uint64_t(tv.tv_sec) * 1000000ULL + tv.tv_usec);
        }
        else
        {
            UAVCAN_ASSERT(0);
            return -1;
        }
#else
        // TODO VSDK-1445 - Enable hardware timestamps on vxworks.
        // VxWorks doesn't support (SOL_SOCKET, SO_TIMESTAMP) in socketCAN. This isn't the best,
        // but this works fine for now.
        ts_utc = clock_.getUtc();
#endif
        return 1;
    }

    void pollWrite()
    {
        while (hasReadyTx())
        {
            const TxItem tx = tx_queue_.top();

            if (tx.deadline >= clock_.getMonotonic())
            {
                const int res = write(tx.frame);
                if (res == 1)                   // Transmitted successfully
                {
                    incrementNumFramesInSocketTxQueue();
                    if ((tx.flags & uavcan::CanIOFlagLoopback) && !loopback_filter_configured_)
                    {
                        (void)pending_loopback_ids_.insert(tx.frame.id);
                    }
                    else if ((tx.flags & uavcan::CanIOFlagLoopback) && loopback_filter_configured_)
                    {
                        UAVCAN_ASSERT(0);
                    }
                }
                else if (res == 0)              // Not transmitted, nor is it an error
                {
                    break;                      // Leaving the loop, the frame remains enqueued for the next retry
                }
                else                            // Transmission error
                {
                    registerError(SocketCanError::SocketWriteFailure);
                }
            }
            else
            {
                registerError(SocketCanError::TxTimeout);
            }

            // Removing the frame from the queue even if transmission failed
            tx_queue_.pop();
        }
    }

    void pollRead()
    {
        while (true)
        {
            RxItem rx;
            rx.ts_mono = clock_.getMonotonic();  // Monotonic timestamp is not required to be precise (unlike UTC)
            bool loopback = false;
            const int res = read(rx.frame, rx.ts_utc, loopback);
            if (res == 1)
            {
                UAVCAN_ASSERT(!rx.ts_utc.isZero());
                bool accept = true;
                if (loopback)                   // We receive loopback for all CAN frames
                {
                    confirmSentFrame();
                    rx.flags |= uavcan::CanIOFlagLoopback;
                    accept = wasInPendingLoopbackSet(rx.frame); // Do we need to send this loopback into the lib?
                }
                if (accept)
                {
                    rx.ts_utc += clock_.getAdjustUtc();
                    rx_queue_.push(rx);
                }
            }
            else if (res == 0)
            {
                break;
            }
            else
            {
                registerError(SocketCanError::SocketReadFailure);
                break;
            }
        }
    }

    /**
     * Returns true if a frame accepted by SW filters
     */
    bool checkSWFilters(const can_frame& frame) const
    {
        if (!sw_filters_container_.empty())
        {
            for (auto& f : sw_filters_container_)
            {
                if (((frame.can_id & f.can_mask) ^ f.can_id) == 0)
                {
                    return true;
                }
            }
            return false;
        }
        else
        {
            return true;
        }
    }

public:
    /**
     * Takes ownership of socket's file descriptor.
     *
     * @ref max_frames_in_socket_tx_queue       See a note in the class comment.
     */
    SocketCanIface(const uavcan_posix::ISystemClock& clock, int socket_fd, int max_frames_in_socket_tx_queue = 2)
        : clock_(clock)
        , fd_(socket_fd)
        , max_frames_in_socket_tx_queue_(max_frames_in_socket_tx_queue)
    {
        UAVCAN_ASSERT(fd_ >= 0);
    }

    /**
     * Socket file descriptor will be closed.
     */
    virtual ~SocketCanIface()
    {
        UAVCAN_TRACE("SocketCAN", "SocketCanIface: Closing fd %d", fd_);
        (void)::close(fd_);
    }

    /**
     * Assumes that the socket is writeable
     */
    std::int16_t send(const uavcan::CanFrame& frame, const uavcan::MonotonicTime tx_deadline,
                      const uavcan::CanIOFlags flags) override
    {
        if ((flags & uavcan::CanIOFlagLoopback) && loopback_filter_configured_)
        {
            UAVCAN_TRACE("SocketCAN", "SocketCanIface: Attempted to Send Loopback Frame with Filters Configured");
            return -1;
        }

        tx_queue_.emplace(frame, tx_deadline, flags, tx_frame_counter_);
        tx_frame_counter_++;
        pollRead();     // Read poll is necessary because it can release the pending TX flag
        pollWrite();
        return 1;
    }

    /**
     * Will read the socket only if RX queue is empty.
     * Normally, poll() needs to be executed first.
     */
    std::int16_t receive(uavcan::CanFrame& out_frame, uavcan::MonotonicTime& out_ts_monotonic,
                         uavcan::UtcTime& out_ts_utc, uavcan::CanIOFlags& out_flags) override
    {
        if (rx_queue_.empty())
        {
            pollRead();            // This allows to use the socket not calling poll() explicitly.
            if (rx_queue_.empty())
            {
                return 0;
            }
        }
        {
            const RxItem& rx = rx_queue_.front();
            out_frame        = rx.frame;
            out_ts_monotonic = rx.ts_mono;
            out_ts_utc       = rx.ts_utc;
            out_flags        = rx.flags;
        }
        rx_queue_.pop();
        return 1;
    }

    /**
     * Performs socket read/write.
     * @param read  Socket is readable
     * @param write Socket is writeable
     */
    void poll(bool read, bool write)
    {
        if (read)
        {
            pollRead();  // Read poll must be executed first because it may decrement frames_in_socket_tx_queue_
        }
        if (write)
        {
            pollWrite();
        }
    }

    bool hasReadyRx() const { return !rx_queue_.empty(); }
    bool hasReadyTx() const
    {
        return !tx_queue_.empty() && (frames_in_socket_tx_queue_ < max_frames_in_socket_tx_queue_);
    }

    std::int16_t configureFilters(const uavcan::CanFilterConfig* const filter_configs,
                                  const std::uint16_t num_configs) override
    {
        if (filter_configs == nullptr && num_configs != 0)
        {
            UAVCAN_ASSERT(0);
            return -1;
        }

        if (num_configs > getNumFilters())
        {
            UAVCAN_ASSERT(0);
            return -1;
        }

        sw_filters_container_.clear();
        sw_filters_container_.resize(num_configs);

        if (num_configs == 0)
        {
#ifndef __VXWORKS__
            //The SocketCAN spec indicates that a zero sized filter array can
            // be used to ignore all ingress CAN frames.
            if (setsockopt(fd_, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0) == 0)
            {
                loopback_filter_configured_ = true;
            }
            else
            {
                UAVCAN_TRACE("SocketCAN", "SocketCanIface: Failed to Configure Socket Filters");
                UAVCAN_ASSERT(0);
                return -1;
            }
#else
            //On VxWorks, setting a zero sized array does not work as expected.
            // So until VxWorks provides a fix, use the CAN_RAW_XMIT_ONLY socket
            // option to achieve the same effect.
            const int optval = 1;
            if (setsockopt(fd_, SOL_CAN_RAW, CAN_RAW_XMIT_ONLY, &optval, sizeof(int)) == 0)
            {
                loopback_filter_configured_ = true;
            }
            else
            {
                UAVCAN_TRACE("SocketCAN", "SocketCanIface: Failed to enable CAN_RAW_XMIT_ONLY sockopt.");
                UAVCAN_ASSERT(0);
                return -1;
            }
#endif
            return 0;
        }

        for (unsigned i = 0; i < num_configs; i++)
        {
            const uavcan::CanFilterConfig& fc = filter_configs[i];
            sw_filters_container_[i].can_id   = fc.id   & uavcan::CanFrame::MaskExtID;
            sw_filters_container_[i].can_mask = fc.mask & uavcan::CanFrame::MaskExtID;
            if (fc.id & uavcan::CanFrame::FlagEFF)
            {
                sw_filters_container_[i].can_id |= CAN_EFF_FLAG;
            }
            if (fc.id & uavcan::CanFrame::FlagRTR)
            {
                sw_filters_container_[i].can_id |= CAN_RTR_FLAG;
            }
            if (fc.mask & uavcan::CanFrame::FlagEFF)
            {
                sw_filters_container_[i].can_mask |= CAN_EFF_FLAG;
            }
            if (fc.mask & uavcan::CanFrame::FlagRTR)
            {
                sw_filters_container_[i].can_mask |= CAN_RTR_FLAG;
            }

            rfilter_[i].can_id   = sw_filters_container_[i].can_id;
            rfilter_[i].can_mask = sw_filters_container_[i].can_mask;
        }

        if (setsockopt(fd_, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter_, sizeof(can_filter)*num_configs) == 0)
        {
            loopback_filter_configured_ = true;
        }
        else
        {
            UAVCAN_TRACE("SocketCAN", "SocketCanIface: Failed to Configure Socket Filters");
            UAVCAN_ASSERT(0);
            return -1;
        }

        return 0;
    }

    /**
     * SocketCAN emulates the CAN filters in software, so the number of filters is virtually unlimited.
     * This method returns a constant value.
     */
    static constexpr unsigned NumFilters = 50;
    struct can_filter rfilter_[NumFilters];
    std::uint16_t getNumFilters() const override { return NumFilters; }


    /**
     * Returns total number of errors of each kind detected since the object was created.
     */
    std::uint64_t getErrorCount() const override
    {
        std::uint64_t ec = 0;
        for (auto& kv : errors_) { ec += kv.second; }
        return ec;
    }

    /**
     * Returns number of errors of each kind in a map.
     */
    const decltype(errors_) & getErrors() const { return errors_; }

    int getFileDescriptor() const { return fd_; }

    /**
     * Open and configure a CAN socket on iface specified by name.
     * @param iface_name String containing iface name, e.g. "can0", "vcan1", "slcan0"
     * @return Socket descriptor or negative number on error.
     */
    static int openSocket(const std::string& iface_name)
    {
        errno = 0;

        const int s = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (s < 0)
        {
            return s;
        }

        const int canfd_on = 1;
        const int canfd_result = setsockopt(s, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &canfd_on, sizeof(canfd_on));

#ifdef __VXWORKS__
        // TODO VSDK-xxxx: return 0 on vxworks
        (void)canfd_result;
#else
        if (canfd_result != 0)
        {
            return 0;
        }
#endif
        class RaiiCloser
        {
            int fd_;
        public:
            RaiiCloser(int filedesc) : fd_(filedesc)
            {
                UAVCAN_ASSERT(fd_ >= 0);
            }
            ~RaiiCloser()
            {
                if (fd_ >= 0)
                {
                    UAVCAN_TRACE("SocketCAN", "RaiiCloser: Closing fd %d", fd_);
                    (void)::close(fd_);
                }
            }
            void disarm() { fd_ = -1; }
        } raii_closer(s);

        // Detect the iface index
        auto ifr = ::ifreq();
        if (iface_name.length() >= IFNAMSIZ)
        {
            errno = ENAMETOOLONG;
            return -1;
        }
        (void)std::strncpy(ifr.ifr_name, iface_name.c_str(), iface_name.length());
        if (::ioctl(s, SIOCGIFINDEX, &ifr) < 0 || ifr.ifr_ifindex < 0)
        {
            return -1;
        }

        // Bind to the specified CAN iface
        {
            auto addr = ::sockaddr_can();
            addr.can_family = AF_CAN;
            addr.can_ifindex = ifr.ifr_ifindex;
            if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
            {
                return -1;
            }
        }

        // Configure
        {
            const int on = 1;
#ifndef __VXWORKS__
            // Timestamping
            // TODO VSDK-xxxx, WindRiver Enhancement Request pending.
            if (::setsockopt(s, SOL_SOCKET, SO_TIMESTAMP, &on, sizeof(on)) < 0)
            {
                return -1;
            }
#endif
            // Socket loopback
            if (::setsockopt(s, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &on, sizeof(on)) < 0)
            {
                return -1;
            }
#ifndef __VXWORKS__
            // Non-blocking
            if (::fcntl(s, F_SETFL, O_NONBLOCK) < 0)
            {
                return -1;
            }
#endif
        }

        // Validate the resulting socket
        {
            int socket_error = 0;
            ::socklen_t errlen = sizeof(socket_error);
            (void)::getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<void*>(&socket_error), &errlen);
            if (socket_error != 0)
            {
                errno = socket_error;
                return -1;
            }
        }

        raii_closer.disarm();
        return s;
    }
};

} // end namespace uavcan_posix
