/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/**
 * @defgroup examples Examples
 *
 * Examples are provided as documentation for how libuavcan can be implemented
 * for real systems.
 *
 * @{
 * @file
 * Implements just the media layer of libuavcan on top of <a
 * href="https://www.kernel.org/doc/Documentation/networking/can.txt">SocketCAN</a>.
 *
 * @warning This is a naive and simplistic implementation. While it may be suitable as prototype
 * it should not be used as an example of how to implement the media layer optimally nor
 * it is tested with any rigor so bugs may exists even while the libuavcan build is passing.
 */

#include <array>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <memory>

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/transport/media/interfaces.hpp"
#include "libuavcan/transport/media/can.hpp"

namespace
{
using CanFrame = libuavcan::transport::media::CAN::Frame<libuavcan::transport::media::CAN::Type2_0::MaxFrameSizeBytes>;
using CanInterfaceManager = libuavcan::transport::media::InterfaceManager<CanFrame>;
using CanInterface        = libuavcan::transport::media::Interface<CanFrame>;
using CanFilterConfig     = CanFrame::Filter;

class SocketCANInterface : public CanInterface
{
private:
    const std::uint_fast16_t        index_;
    const int                       fd_;

public:
    SocketCANInterface(std::uint_fast16_t index, int fd)
        : index_(index)
        , fd_(fd)
    {}

    virtual ~SocketCANInterface()
    {
        std::cout << "closing socket." << std::endl;
        ::close(fd_);
    }

    virtual std::uint_fast16_t getInterfaceIndex() const override
    {
        return index_;
    }

    virtual libuavcan::Result enqueue(const CanFrame& frame, libuavcan::time::Monotonic tx_deadline) override
    {
        return enqueue_internal(frame, &tx_deadline);
    }

    virtual libuavcan::Result enqueue(const CanFrame& frame) override
    {
        return enqueue_internal(frame, nullptr);
    }

    virtual libuavcan::Result popBack(CanFrame& out_frame) override
    {
        ::iovec iov;
        ::can_frame socketcan_frame;
        // TODO CAN-FD
        iov.iov_base = &socketcan_frame;
        iov.iov_len  = sizeof(socketcan_frame);

        static constexpr size_t ControlSize = sizeof(cmsghdr) + sizeof(::timeval);
        using ControlStorage = typename std::aligned_storage<ControlSize>::type;
        ControlStorage control_storage;
        std::uint8_t* control = reinterpret_cast<std::uint8_t *>(&control_storage);
        std::fill(control, control + ControlSize, 0x00);

        ::msghdr msg;
        msg.msg_iov    = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = control;
        msg.msg_controllen = ControlSize;

        const auto res = ::recvmsg(fd_, &msg, MSG_DONTWAIT);
        if (res <= 0)
        {
            return (res < 0 && errno == EWOULDBLOCK) ? 0 : -1;
        }

        (void)out_frame;
        // TODO
        //frame = makeUavcanFrame(sockcan_frame);

        return 1;
    }

private:
    inline libuavcan::Result enqueue_internal(const CanFrame& frame, libuavcan::time::Monotonic* optional_deadline)
    {
        // This is a dumb implementation for example purposes only. We ignore deadlines and don't actually
        // enqueue anything. This is also not a non-blocking implementation so all around this example is deficient.
        (void)optional_deadline;
        errno = 0;

        ::can_frame socketcan_frame;
        // All UAVCAN frames use the extended frame format.
        socketcan_frame.can_id = CAN_EFF_FLAG | (frame.id & CanFrame::MaskExtID);
        socketcan_frame.can_dlc = static_cast<std::underlying_type<libuavcan::transport::media::CAN::FrameDLC>::type>(frame.getDLC());
        std::copy(frame.data, frame.data + frame.getDataLength(), socketcan_frame.data);
        // TODO ::canfd_frame CANFD_BRS and CANFD_ESI

        const auto res = ::write(fd_, &socketcan_frame, sizeof(socketcan_frame));
        if (res <= 0)
        {
            if (errno == ENOBUFS || errno == EAGAIN)    // Writing is not possible atm
            {
                return -1;
            }
            return -2;
        }
        if (static_cast<std::size_t>(res) != sizeof(socketcan_frame))
        {
            return -3;
        }
        return 0;
    }
};

/**
 * This datastruture is part of the SocketCANInterfaceManager's internal
 * interface management storage.
 */
struct InterfaceRecord final
{
    InterfaceRecord(const InterfaceRecord&) = delete;
    const InterfaceRecord& operator=(const InterfaceRecord&) = delete;

    InterfaceRecord()
        : name()
        , connected_interface(nullptr)
    {}

    InterfaceRecord(const char* cname)
        : name(cname)
        , connected_interface(nullptr)
    {}

    InterfaceRecord(InterfaceRecord&& rhs)
        : name(std::move(rhs.name))
        , connected_interface(rhs.connected_interface)
    {
        rhs.connected_interface = nullptr;
    }

    const std::string   name;
    SocketCANInterface* connected_interface;
};

/**
 * For higher-level systems where interfaces may come and go the manager pattern allows a central
 * authority to monitor hardware availability and to provide a single location for interface lifecycle
 * management. For low-level systems this could be a very simple and static object that assumes interfaces
 * are never closed.
 */
class SocketCANInterfaceManager : public CanInterfaceManager
{
private:
    std::vector<InterfaceRecord> interface_list_;

public:
    SocketCANInterfaceManager(const SocketCANInterfaceManager&) = delete;
    SocketCANInterfaceManager(SocketCANInterfaceManager&&)      = delete;
    SocketCANInterfaceManager& operator=(const SocketCANInterfaceManager&)   = delete;
    SocketCANInterfaceManager& operator&&(const SocketCANInterfaceManager&&) = delete;

    SocketCANInterfaceManager()
        : interface_list_()
    {}

    virtual ~SocketCANInterfaceManager()
    {
        for (const auto& ir : interface_list_)
        {
            if (nullptr != ir.connected_interface)
            {
                std::cout << "Interface " << ir.name << " was still open when the manager was destroyed?!" << std::endl;
            }
        }
    }

    virtual libuavcan::Result openInterface(std::uint_fast16_t     interface_index,
                                            const CanFilterConfig* filter_config,
                                            std::size_t            filter_config_length,
                                            CanInterface*&         out_interface) override
    {
        if (interface_index >= interface_list_.size())
        {
            return -1;
        }
        InterfaceRecord& ir = interface_list_[interface_index];
        const int        fd = openSocket(ir.name, false);
        if (fd <= 0)
        {
            return -2;
        }
        if (0 != configureFilters(fd, filter_config, filter_config_length))
        {
            return -3;
        }
        ir.connected_interface = new SocketCANInterface(interface_index, fd);
        if (nullptr == ir.connected_interface)
        {
            // If compiling without c++ exceptions new can return null if OOM.
            ::close(fd);
            return -4;
        }
        out_interface = ir.connected_interface;
        return 0;
    }

    virtual libuavcan::Result closeInterface(CanInterface*& inout_interface) override
    {
        if (nullptr != inout_interface)
        {
            InterfaceRecord& ir = interface_list_[inout_interface->getInterfaceIndex()];
            delete ir.connected_interface;
            ir.connected_interface = nullptr;
            inout_interface        = nullptr;
        }
        return -1;
    }

    virtual std::size_t getHardwareInterfaceCount() const override
    {
        return interface_list_.size();
    }

    virtual std::size_t getMaxHardwareFrameFilters(std::uint_fast16_t interface_index) const override
    {
        // We assume that the underlying driver does not use hardware filters. This
        // is not necessarily the case but assume the worst.
        (void) interface_index;
        return 0;
    }

    virtual std::size_t getMaxFrameFilters(std::uint_fast16_t interface_index) const override
    {
        (void) interface_index;
        return std::numeric_limits<std::size_t>::max();
    }

    const std::string& getInterfaceName(std::size_t interface_index) const
    {
        return interface_list_[interface_index].name;
    }

    const std::string& getInterfaceName(CanInterface& interface) const
    {
        return getInterfaceName(interface.getInterfaceIndex());
    }

    std::size_t reenumerateInterfaces()
    {
        interface_list_.clear();

        struct ifaddrs* ifap;
        if (0 == ::getifaddrs(&ifap))
        {
            std::cout << "Got ifaddrs" << std::endl;
            struct ifaddrs* i = ifap;
            while (i)
            {
                if (0 == std::strncmp("vcan", i->ifa_name, 4))
                {
                    std::cout << "Found vcan adapter " << i->ifa_name << std::endl;
                    interface_list_.emplace_back(i->ifa_name);
                }
                i = i->ifa_next;
            }
            ::freeifaddrs(ifap);
        }
        return interface_list_.size();
    }

private:
    std::int16_t configureFilters(const int                    fd,
                                  const CanFilterConfig* const filter_configs,
                                  const std::size_t            num_configs)
    {
        if (filter_configs == nullptr && num_configs != 0)
        {
            return -1;
        }

        std::vector<::can_filter> socket_filters;

        if (num_configs == 0)
        {
            // The SocketCAN spec indicates that a zero sized filter array can
            // be used to ignore all ingress CAN frames.
            if (0 != setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, nullptr, 0))
            {
                return -1;
            }
            return 0;
        }

        // TODO
        //   4.1.1.1 CAN filter usage optimisation

        //   The CAN filters are processed in per-device filter lists at CAN frame
        //   reception time. To reduce the number of checks that need to be performed
        //   while walking through the filter lists the CAN core provides an optimized
        //   filter handling when the filter subscription focusses on a single CAN ID.

        //   For the possible 2048 SFF CAN identifiers the identifier is used as an index
        //   to access the corresponding subscription list without any further checks.
        //   For the 2^29 possible EFF CAN identifiers a 10 bit XOR folding is used as
        //   hash function to retrieve the EFF table index.

        //   To benefit from the optimized filters for single CAN identifiers the
        //   CAN_SFF_MASK or CAN_EFF_MASK have to be set into can_filter.mask together
        //   with set CAN_EFF_FLAG and CAN_RTR_FLAG bits. A set CAN_EFF_FLAG bit in the
        //   can_filter.mask makes clear that it matters whether a SFF or EFF CAN ID is
        //   subscribed. E.g. in the example from above

        //     rfilter[0].can_id   = 0x123;
        //     rfilter[0].can_mask = CAN_SFF_MASK;

        //   both SFF frames with CAN ID 0x123 and EFF frames with 0xXXXXX123 can pass.

        //   To filter for only 0x123 (SFF) and 0x12345678 (EFF) CAN identifiers the
        //   filter has to be defined in this way to benefit from the optimized filters:

        //     struct can_filter rfilter[2];

        //     rfilter[0].can_id   = 0x123;
        //     rfilter[0].can_mask = (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_SFF_MASK);
        //     rfilter[1].can_id   = 0x12345678 | CAN_EFF_FLAG;
        //     rfilter[1].can_mask = (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_EFF_MASK);

        //     setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));

        for (unsigned i = 0; i < num_configs; i++)
        {
            const CanFilterConfig& fc = filter_configs[i];
            socket_filters.emplace_back(
                std::move<::can_filter>({fc.id & CanFrame::MaskExtID, fc.mask & CanFrame::MaskExtID}));
        }

        static_assert(sizeof(socklen_t) <= sizeof(std::size_t) &&
                          std::is_signed<socklen_t>::value == std::is_signed<std::size_t>::value,
                      "socklen_t is not of the expected integer type?");

        if (0 != setsockopt(fd,
                            SOL_CAN_RAW,
                            CAN_RAW_FILTER,
                            socket_filters.data(),
                            static_cast<socklen_t>(sizeof(can_filter) * socket_filters.size())))
        {
            return -1;
        }

        return 0;
    }

    /**
     * Open and configure a CAN socket on iface specified by name.
     * @param iface_name String containing iface name, e.g. "can0", "vcan1", "slcan0"
     * @return Socket descriptor or negative number on error.
     */
    static int openSocket(const std::string& iface_name, bool enable_canfd)
    {
        errno = 0;

        const int s = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (s < 0)
        {
            return s;
        }

        if (enable_canfd)
        {
            const int canfd_on     = 1;
            const int canfd_result = setsockopt(s, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &canfd_on, sizeof(canfd_on));

            if (canfd_result != 0)
            {
                return 0;
            }
        }

        class RaiiCloser
        {
            int fd_;

        public:
            RaiiCloser(int filedesc)
                : fd_(filedesc)
            {
                LIBUAVCAN_ASSERT(fd_ >= 0);
            }
            ~RaiiCloser()
            {
                if (fd_ >= 0)
                {
                    (void) ::close(fd_);
                }
            }
            void disarm()
            {
                fd_ = -1;
            }
        } raii_closer(s);

        // Detect the iface index
        auto ifr = ::ifreq();
        if (iface_name.length() >= IFNAMSIZ)
        {
            errno = ENAMETOOLONG;
            return -1;
        }
        (void) std::strncpy(ifr.ifr_name, iface_name.c_str(), iface_name.length());
        if (::ioctl(s, SIOCGIFINDEX, &ifr) < 0 || ifr.ifr_ifindex < 0)
        {
            return -1;
        }

        // Bind to the specified CAN iface
        {
            auto addr        = ::sockaddr_can();
            addr.can_family  = AF_CAN;
            addr.can_ifindex = ifr.ifr_ifindex;
            if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
            {
                return -1;
            }
        }

        // Configure
        {
            const int on = 1;

            // Timestamping
            if (::setsockopt(s, SOL_SOCKET, SO_TIMESTAMP, &on, sizeof(on)) < 0)
            {
                return -1;
            }
            // Socket loopback
            if (::setsockopt(s, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &on, sizeof(on)) < 0)
            {
                return -1;
            }
        }

        // Validate the resulting socket
        {
            int         socket_error = 0;
            ::socklen_t errlen       = sizeof(socket_error);
            (void) ::getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<void*>(&socket_error), &errlen);
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

}  // namespace

/** @} */  // end of examples group

int main(int argc, char* argv[])
{
    (void) argc;
    (void) argv;
    std::cout << "starting up." << std::endl;
    SocketCANInterfaceManager manager;
    const std::size_t         found_count = manager.reenumerateInterfaces();
    std::cout << "Found " << found_count << " interfaces." << std::endl;

    for (std::size_t i = 0, if_count = manager.getHardwareInterfaceCount(); i < if_count; ++i)
    {
        CanInterface* interface_ptr;
        if (0 == manager.openInterface(i, nullptr, 0, interface_ptr))
        {
            // demonstration of how to convert the media layer APIs into RAII patterns.
            auto interface_deleter = [&](CanInterface* interface) { manager.closeInterface(interface); };
            std::unique_ptr<CanInterface, decltype(interface_deleter)> interface(interface_ptr, interface_deleter);
            std::cout << "Opened interface " << manager.getInterfaceName(i) << std::endl;

            CanFrame test_frame{1, libuavcan::time::Monotonic::fromMicrosecond(0), nullptr, libuavcan::transport::media::CAN::FrameDLC::CodeForLength0};

            if (0 <= interface_ptr->enqueue(test_frame))
            {
                std::cout << "Successfully enqueued a frame on " << manager.getInterfaceName(*interface_ptr) << std::endl;
            }
            else
            {
                std::cout << "Failed to enqueue a frame on " << manager.getInterfaceName(*interface_ptr) << std::endl;
            }

        }
        else
        {
            std::cout << "Failed to open interface " << manager.getInterfaceName(i) << std::endl;
        }
    }
    return 0;
}
