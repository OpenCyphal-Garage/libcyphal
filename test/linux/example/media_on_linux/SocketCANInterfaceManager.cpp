/*
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/**
 * @defgroup examples Examples
 *
 * @{
 */
#include <iostream>
#include <cstring>
#include <memory>

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <fcntl.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/can/error.h>
#include <linux/net_tstamp.h>

#include "SocketCANInterfaceManager.hpp"

namespace libcyphal
{
namespace example
{
// +--------------------------------------------------------------------------+
// | INTERNAL METHODS AND TYPES
// +--------------------------------------------------------------------------+
namespace
{
void socketDeleter(int* socket)
{
    if (nullptr != socket)
    {
        (void) ::close(*socket);
    }
}

using RaiiSocket = std::unique_ptr<int, decltype(&socketDeleter)>;

/**
 * Open and configure a CAN socket on iface specified by name.
 * @param  iface_name String containing iface name, e.g. "can0", "vcan1", "slcan0"
 * @param  enable_canfd If true then the method will attempt to enable can-fd for the interface.
 * @param  enable_receive_own_messages  If true then the socket will also receive any messages sent
 *         from this process. This is normally only useful for testing.
 * @return Socket descriptor or negative number on error.
 */
inline int openSocket(const std::string& iface_name, bool enable_canfd, bool enable_receive_own_messages)
{
    errno = 0;

    int s = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0)
    {
        return s;
    }

    RaiiSocket raii_closer(&s, &socketDeleter);

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
            LIBCYPHAL_TRACEF("SocketCANInterfaceManager",
                             "SO_TIMESTAMP was not supported for socket %s",
                             iface_name.c_str());
        }

        int enable_rxq_ovfl = 1;
        if (::setsockopt(s, SOL_SOCKET, SO_RXQ_OVFL, &enable_rxq_ovfl, sizeof(enable_rxq_ovfl)) < 0)
        {
            LIBCYPHAL_TRACEF("SocketCANInterfaceManager",
                             "SO_RXQ_OVFL was not supported for socket %s",
                             iface_name.c_str());
        }
        ::can_err_mask_t err_mask = CAN_ERR_MASK;
        if (::setsockopt(s, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &err_mask, sizeof(err_mask)) < 0)
        {
            LIBCYPHAL_TRACEF("SocketCANInterfaceManager",
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

}  // namespace

// +--------------------------------------------------------------------------+
// | SocketCANInterfaceManager IMPLEMENTATION
// +--------------------------------------------------------------------------+

SocketCANInterfaceManager::SocketCANInterfaceManager(const std::vector<std::string>&& required_interfaces,
                                                     bool                             enable_can_fd,
                                                     bool                             receive_own_messages)
    : required_interfaces_(required_interfaces)
    , enable_can_fd_(enable_can_fd)
    , receive_own_messages_(receive_own_messages)
{}

libcyphal::Result SocketCANInterfaceManager::startInterfaceGroup(
    const InterfaceGroupType::FrameType::Filter* filter_config,
    std::size_t                                  filter_config_length,
    InterfaceGroupPtrType&                       out_group)
{
    std::vector<std::unique_ptr<SocketCANInterfaceGroup::InterfaceType>> interfaces;
    interfaces.reserve(required_interfaces_.size());

    for (std::uint_fast8_t i = 0; i < required_interfaces_.size(); ++i)
    {
        std::unique_ptr<SocketCANInterfaceGroup::InterfaceType> interface;
        if (!!createInterface(i, required_interfaces_[i], filter_config, filter_config_length, interface))
        {
            interfaces.emplace_back(std::move(interface));
        }
    }
    if (interfaces.size() == 0)
    {
        return libcyphal::Result::NotFound;
    }
    else
    {
        out_group.reset(new SocketCANInterfaceGroup(std::move(interfaces)));
        return (out_group->getInterfaceCount() >= required_interfaces_.size()) ? libcyphal::Result::Success
                                                                               : libcyphal::Result::SuccessPartial;
    }
}

libcyphal::Result SocketCANInterfaceManager::stopInterfaceGroup(std::shared_ptr<SocketCANInterfaceGroup>& out_group)
{
    // This implementation doesn't do anything meaningful on stop other than deleing the group.
    // We provide this lifecycle formality for any system where a centralized object must own the
    // group interface memory.
    out_group = nullptr;
    return libcyphal::Result::Success;
}

bool SocketCANInterfaceManager::doesReceiveOwnMessages() const
{
    return receive_own_messages_;
}

libcyphal::Result SocketCANInterfaceManager::createInterface(
    std::uint_fast8_t                                        interface_index,
    const std::string&                                       interface_name,
    const SocketCANInterfaceGroup::FrameType::Filter*        filter_config,
    std::size_t                                              filter_config_length,
    std::unique_ptr<SocketCANInterfaceGroup::InterfaceType>& out_interface)
{
    int socket_descriptor = openSocket(interface_name, enable_can_fd_, receive_own_messages_);
    if (socket_descriptor <= 0)
    {
        return libcyphal::Result::UnknownInternalError;
    }
    RaiiSocket raii_closer(&socket_descriptor, &socketDeleter);
    const auto result = SocketCANInterfaceGroup::configureFilters(socket_descriptor, filter_config, filter_config_length);
    if (!result)
    {
        return result;
    }
    out_interface.reset(new SocketCANInterfaceGroup::InterfaceType(interface_index, interface_name, socket_descriptor));
    if (!out_interface)
    {
        // If compiling without c++ exceptions new can return null if OOM.
        return libcyphal::Result::OutOfMemory;
    }
    else
    {
        raii_closer.release();
    }
    return libcyphal::Result::Success;
}

std::size_t SocketCANInterfaceManager::getMaxFrameFilters() const
{
    // Some arbitrary number that seemed reasonable for socketCAN in 2019. This is just an example implementation
    // so don't assume this constant is generically applicable to to libcyphal::media::InterfaceManager.
    return 512;
}

}  // namespace example
}  // namespace libcyphal

/** @} */  // end of examples group
