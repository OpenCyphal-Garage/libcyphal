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
#include <ifaddrs.h>
#include <memory>
#include <fcntl.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/can/error.h>
#include <linux/net_tstamp.h>

#include "SocketCANInterfaceManager.hpp"

namespace libuavcan
{
namespace example
{
SocketCANInterfaceManager::SocketCANInterfaceManager(bool enable_can_fd, bool receive_own_messages)
    : interface_list_()
    , pollfds_()
    , enable_can_fd_(enable_can_fd)
    , receive_own_messages_(receive_own_messages)
{}

SocketCANInterfaceManager::~SocketCANInterfaceManager()
{
    for (const auto& ir : interface_list_)
    {
        if (nullptr != ir.connected_interface)
        {
            LIBUAVCAN_TRACEF("SocketCANInterfaceManager",
                             "Interface %s was still open when the manager was destroyed?!",
                             ir.name.c_str());
        }
    }
}

libuavcan::Result SocketCANInterfaceManager::openInterface(std::uint_fast8_t                       interface_index,
                                                           const InterfaceType::FrameType::Filter* filter_config,
                                                           std::size_t                             filter_config_length,
                                                           InterfaceType*&                         out_interface)
{
    if (interface_index >= interface_list_.size())
    {
        return libuavcan::Result::BadArgument;
    }
    InterfaceRecord<InterfaceType>& ir = interface_list_[interface_index];
    if (!ir.connected_interface)
    {
        const int fd = openSocket(ir.name, enable_can_fd_, receive_own_messages_);
        if (fd <= 0)
        {
            return libuavcan::Result::UnknownInternalError;
        }
        const auto result = configureFilters(fd, filter_config, filter_config_length);
        if (!result)
        {
            return result;
        }
        ir.connected_interface.reset(new InterfaceType(interface_index, fd));
        if (!ir.connected_interface)
        {
            // If compiling without c++ exceptions new can return null if OOM.
            ::close(fd);
            return libuavcan::Result::OutOfMemory;
        }
    }
    out_interface = ir.connected_interface.get();
    return libuavcan::Result::Success;
}

libuavcan::Result SocketCANInterfaceManager::closeInterface(InterfaceType*& inout_interface)
{
    if (nullptr != inout_interface)
    {
        InterfaceRecord<InterfaceType>& ir = interface_list_[inout_interface->getInterfaceIndex()];
        if (inout_interface != ir.connected_interface.get())
        {
            return libuavcan::Result::BadArgument;
        }
        ir.connected_interface.reset(nullptr);
        inout_interface = nullptr;
    }
    return libuavcan::Result::Success;
}

std::uint_fast8_t SocketCANInterfaceManager::getHardwareInterfaceCount() const
{
    const auto list_size = interface_list_.size();
    // Remember that fast 8 can be > 8 but never < 8. Therefore we should saturate
    // at 8 to provide a consistent behaviour regardless of architecure.
    if (list_size > std::numeric_limits<std::uint8_t>::max())
    {
        return std::numeric_limits<std::uint8_t>::max();
    }
    else
    {
        return static_cast<std::uint_fast8_t>(list_size);
    }
}

std::size_t SocketCANInterfaceManager::getMaxHardwareFrameFilters(std::uint_fast8_t interface_index) const
{
    // We assume that the underlying driver does not use hardware filters. This
    // is not necessarily the case but assume the worst.
    (void) interface_index;
    return 0;
}

std::size_t SocketCANInterfaceManager::getMaxFrameFilters(std::uint_fast8_t interface_index) const
{
    (void) interface_index;
    // Some arbitrary number that seemed reasonable for CAN in 2019. This is just an example implementation
    // so don't assume this constant is generically applicable to to libuavcan::media::InterfaceManager.
    return 512;
}

const std::string& SocketCANInterfaceManager::getInterfaceName(std::size_t interface_index) const
{
    return interface_list_[interface_index].name;
}

const std::string& SocketCANInterfaceManager::getInterfaceName(const InterfaceType& interface) const
{
    return getInterfaceName(interface.getInterfaceIndex());
}

bool SocketCANInterfaceManager::doesReceiveOwnMessages() const
{
    return receive_own_messages_;
}

bool SocketCANInterfaceManager::isFDEnabled() const
{
    return enable_can_fd_;
}

libuavcan::Result SocketCANInterfaceManager::reenumerateInterfaces()
{
    // This is example code and is probably not the best way to discover can interfaces on
    // a system.
    // I believe using the netlink socket layer in linux is the proper way but I didn't
    // have time to figure this out. Contributions welcome.
    interface_list_.clear();

    struct ::ifaddrs* ifap;
    if (0 == ::getifaddrs(&ifap))
    {
        auto iffree = [](struct ifaddrs* p) {
            if (p)
            {
                ::freeifaddrs(p);
            }
        };
        std::unique_ptr<struct ifaddrs, decltype(iffree)> raii_closer(ifap, iffree);

        struct ::ifaddrs* i = ifap;
        while (i)
        {
            if (interface_list_.size() >= std::numeric_limits<std::uint_fast8_t>::max())
            {
                return libuavcan::Result::SuccessPartial;
            }
            const int fd = openSocket(i->ifa_name, enable_can_fd_, receive_own_messages_);
            if (fd > 0)
            {
                ::close(fd);
                LIBUAVCAN_TRACEF("SocketCANInterfaceManager", "Found can socket %s", i->ifa_name);
                interface_list_.emplace_back(i->ifa_name);
            }
            i = i->ifa_next;
        }
    }
    return (interface_list_.size() > 0) ? libuavcan::Result::Success : libuavcan::Result::NotFound;
}

libuavcan::Result SocketCANInterfaceManager::select(const InterfaceType* const (&interfaces)[MaxSelectInterfaces],
                                                    std::size_t                    interfaces_length,
                                                    libuavcan::duration::Monotonic timeout,
                                                    bool                           ignore_write_available)
{
    short int events = POLLIN | POLLPRI;

    if (!ignore_write_available)
    {
        events |= POLLOUT;
    }

    for (size_t i = 0; i < MaxSelectInterfaces && i < interfaces_length; ++i)
    {
        const InterfaceType* const interface = interfaces[i];
        if (interface)
        {
            pollfds_[i] = {interface->getFd(), events, 0};
        }
        else
        {
            return libuavcan::Result::BadArgument;
        }
    }

    ::timespec timeout_spec = {timeout.toMicrosecond() / 1000000U, 0};
    timeout_spec.tv_nsec =
        (timeout - libuavcan::duration::Monotonic::fromMicrosecond(timeout_spec.tv_sec * 1000000U)).toMicrosecond();

    const int result = ::ppoll(pollfds_, interfaces_length, &timeout_spec, nullptr);

    if (0 == result)
    {
        return libuavcan::Result::SuccessTimeout;
    }

    if (0 > result)
    {
        return libuavcan::Result::Failure;
    }

    for (size_t i = 0; i < MaxSelectInterfaces && i < interfaces_length; ++i)
    {
        if (0 != (pollfds_[i].revents & (POLLPRI | POLLERR | POLLHUP | POLLNVAL)))
        {
            return libuavcan::Result::SuccessPartial;
        }
    }
    return libuavcan::Result::Success;
}

libuavcan::Result SocketCANInterfaceManager::getInterfaceIndex(const std::string& interface_name,
                                                               std::uint_fast8_t& out_index) const
{
    for (std::size_t i = 0; i < interface_list_.size(); ++i)
    {
        if (i > std::numeric_limits<std::uint_fast8_t>::max())
        {
            break;
        }
        if (interface_list_[i].name == interface_name)
        {
            out_index = static_cast<std::uint_fast8_t>(i);
            return libuavcan::Result::Success;
        }
    }
    return libuavcan::Result::NotFound;
}

libuavcan::Result SocketCANInterfaceManager::configureFilters(
    const int                                     fd,
    const InterfaceType::FrameType::Filter* const filter_configs,
    const std::size_t                             num_configs)
{
    if (filter_configs == nullptr && num_configs != 0 && num_configs <= CAN_RAW_FILTER_MAX)
    {
        return libuavcan::Result::BadArgument;
    }

    std::vector<::can_filter> socket_filters;

    if (num_configs == 0)
    {
        // The SocketCAN spec indicates that a zero sized filter array can
        // be used to ignore all ingress CAN frames.
        if (0 != setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, nullptr, 0))
        {
            return libuavcan::Result::UnknownInternalError;
        }
        return libuavcan::Result::Success;
    }

    for (unsigned i = 0; i < num_configs; i++)
    {
        const InterfaceType::FrameType::Filter& fc = filter_configs[i];
        // Use CAN_EFF_FLAG to let the kernel know this is an EFF filter.
        socket_filters.emplace_back(::can_filter{(fc.id & InterfaceType::FrameType::MaskExtID) | CAN_EFF_FLAG,  //
                                                 fc.mask | CAN_EFF_FLAG});
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
        return libuavcan::Result::UnknownInternalError;
    }

    return libuavcan::Result::Success;
}

int SocketCANInterfaceManager::openSocket(const std::string& iface_name,
                                          bool               enable_canfd,
                                          bool               enable_receive_own_messages)
{
    errno = 0;

    const int s = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0)
    {
        return s;
    }

    auto socket_deleter = [](const int* socket) {
        if (nullptr != socket)
        {
            (void) ::close(*socket);
        }
    };

    std::unique_ptr<const int, decltype(socket_deleter)> raii_closer(&s, socket_deleter);

    if (enable_canfd)
    {
        const int canfd_on     = 1;
        const int canfd_result = ::setsockopt(s, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &canfd_on, sizeof(canfd_on));

        if (canfd_result != 0)
        {
            return -1;
        }
    }

    if (enable_receive_own_messages)
    {
        const int receive_own_messages = 1;

        const int receive_own_messages_result =
            ::setsockopt(s, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &receive_own_messages, sizeof(receive_own_messages));

        if (receive_own_messages_result != 0)
        {
            return -1;
        }
    }

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
        const int enable_timestamps = 1;
        if (::setsockopt(s, SOL_SOCKET, SO_TIMESTAMP, &enable_timestamps, sizeof(enable_timestamps)) < 0)
        {
            LIBUAVCAN_TRACEF("SocketCANInterfaceManager",
                             "SO_TIMESTAMP was not supported for socket %s",
                             iface_name.c_str());
        }

        int enable_rxq_ovfl = 1;
        if (::setsockopt(s, SOL_SOCKET, SO_RXQ_OVFL, &enable_rxq_ovfl, sizeof(enable_rxq_ovfl)) < 0)
        {
            LIBUAVCAN_TRACEF("SocketCANInterfaceManager",
                             "SO_RXQ_OVFL was not supported for socket %s",
                             iface_name.c_str());
        }
        ::can_err_mask_t err_mask = CAN_ERR_MASK;
        if (::setsockopt(s, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &err_mask, sizeof(err_mask)) < 0)
        {
            LIBUAVCAN_TRACEF("SocketCANInterfaceManager",
                             "SOL_CAN_RAW was not supported for socket %s",
                             iface_name.c_str());
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

    raii_closer.release();
    return s;
}

}  // namespace example
}  // namespace libuavcan

/** @} */  // end of examples group
