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
#include <linux/net_tstamp.h>

#include "SocketCANInterfaceManager.hpp"

namespace libuavcan
{
namespace example
{
SocketCANInterfaceManager::SocketCANInterfaceManager(bool enable_can_fd, bool receive_own_messages)
    : interface_list_()
    , enable_can_fd_(enable_can_fd)
    , receive_own_messages_(receive_own_messages)
{}

SocketCANInterfaceManager::~SocketCANInterfaceManager()
{
    for (const auto& ir : interface_list_)
    {
        if (nullptr != ir.connected_interface)
        {
            std::cout << "Interface " << ir.name << " was still open when the manager was destroyed?!" << std::endl;
        }
    }
}

libuavcan::Result SocketCANInterfaceManager::openInterface(std::uint_fast8_t      interface_index,
                                                           const CanFilterConfig* filter_config,
                                                           std::size_t            filter_config_length,
                                                           CanInterface*&         out_interface)
{
    if (interface_index >= interface_list_.size())
    {
        return libuavcan::results::bad_argument;
    }
    InterfaceRecord& ir = interface_list_[interface_index];
    const int        fd = openSocket(ir.name, enable_can_fd_, receive_own_messages_);
    if (fd <= 0)
    {
        return libuavcan::results::unknown_internal_error;
    }
    const auto result = configureFilters(fd, filter_config, filter_config_length);
    if (!result)
    {
        return result;
    }
    ir.connected_interface.reset(new SocketCANInterface(interface_index, fd));
    if (!ir.connected_interface)
    {
        // If compiling without c++ exceptions new can return null if OOM.
        ::close(fd);
        return libuavcan::results::out_of_memory;
    }
    out_interface = ir.connected_interface.get();
    return libuavcan::results::success;
}

libuavcan::Result SocketCANInterfaceManager::closeInterface(CanInterface*& inout_interface)
{
    if (nullptr != inout_interface)
    {
        InterfaceRecord& ir = interface_list_[inout_interface->getInterfaceIndex()];
        ir.connected_interface.reset(nullptr);
        inout_interface = nullptr;
    }
    return libuavcan::results::success;
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
    return std::numeric_limits<std::size_t>::max();
}

const std::string& SocketCANInterfaceManager::getInterfaceName(std::size_t interface_index) const
{
    return interface_list_[interface_index].name;
}

const std::string& SocketCANInterfaceManager::getInterfaceName(CanInterface& interface) const
{
    return getInterfaceName(interface.getInterfaceIndex());
}

std::size_t SocketCANInterfaceManager::reenumerateInterfaces()
{
    interface_list_.clear();

    struct ifaddrs* ifap;
    if (0 == ::getifaddrs(&ifap))
    {
        auto iffree = [](struct ifaddrs* p) {
            if (p)
            {
                ::freeifaddrs(p);
            }
        };
        std::unique_ptr<struct ifaddrs, decltype(iffree)> raii_closer(ifap, iffree);

        struct ifaddrs* i = ifap;
        while (i)
        {
            const int fd = openSocket(i->ifa_name, enable_can_fd_, receive_own_messages_);
            if (fd > 0)
            {
                ::close(fd);
                std::cout << "Found can socket " << i->ifa_name << std::endl;
                interface_list_.emplace_back(i->ifa_name);
            }
            i = i->ifa_next;
        }
    }
    return interface_list_.size();
}

libuavcan::Result SocketCANInterfaceManager::configureFilters(const int                    fd,
                                                              const CanFilterConfig* const filter_configs,
                                                              const std::size_t            num_configs)
{
    if (filter_configs == nullptr && num_configs != 0 && num_configs <= CAN_RAW_FILTER_MAX)
    {
        return libuavcan::results::bad_argument;
    }

    std::vector<::can_filter> socket_filters;

    if (num_configs == 0)
    {
        // The SocketCAN spec indicates that a zero sized filter array can
        // be used to ignore all ingress CAN frames.
        if (0 != setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, nullptr, 0))
        {
            return libuavcan::results::unknown_internal_error;
        }
        return libuavcan::results::success;
    }

    for (unsigned i = 0; i < num_configs; i++)
    {
        const CanFilterConfig& fc = filter_configs[i];
        // Use CAN_EFF_FLAG to let the kernel know this is an EFF filter.
        socket_filters.emplace_back(::can_filter{(fc.id & libuavcan::example::CanFrame::MaskExtID) | CAN_EFF_FLAG,  //
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
        return libuavcan::results::unknown_internal_error;
    }

    return libuavcan::results::success;
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
        const int canfd_result = setsockopt(s, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &canfd_on, sizeof(canfd_on));

        if (canfd_result != 0)
        {
            return -1;
        }
    }

    if (enable_receive_own_messages)
    {
        const int receive_own_messages = 1;

        const int receive_own_messages_result =
            setsockopt(s, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &receive_own_messages, sizeof(receive_own_messages));

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
        const int ts_flags = SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_RX_HARDWARE;

        // Hardware Timestamping
        if (::setsockopt(s, SOL_SOCKET, SO_TIMESTAMPING, &ts_flags, sizeof(ts_flags)) < 0)
        {
            return -1;
        }
        int enable_rxq_ovfl = 1;
        if (setsockopt(s, SOL_SOCKET, SO_RXQ_OVFL, &enable_rxq_ovfl, sizeof(enable_rxq_ovfl)) < 0)
        {
            // TODO: handle this in the APIs. Do we allow operating without this support?
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

    raii_closer.release();
    return s;
}

}  // namespace example
}  // namespace libuavcan

/** @} */  // end of examples group
