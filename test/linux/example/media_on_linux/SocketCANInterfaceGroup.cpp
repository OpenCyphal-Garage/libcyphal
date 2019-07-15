/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/**
 * @defgroup examples Examples
 *
 * @{
 */
#include <iostream>
#include <memory>
#include <cstring>

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

namespace libuavcan
{
namespace example
{
SocketCANInterfaceGroup::SocketCANInterfaceGroup(std::vector<std::unique_ptr<InterfaceType>>&& interfaces)
    : interfaces_(std::move(interfaces))
    , pollfds_()
{}

std::uint_fast8_t SocketCANInterfaceGroup::getInterfaceCount() const
{
    return static_cast<std::uint_fast8_t>(
        std::min(static_cast<std::size_t>(std::numeric_limits<std::uint_fast8_t>::max()), interfaces_.size()));
}

const std::string& SocketCANInterfaceGroup::getInterfaceName(std::uint_fast8_t index) const
{
    static const std::string no_name("<bad index>");
    if (index >= interfaces_.size())
    {
        return no_name;
    }
    else
    {
        return interfaces_[index]->getInterfaceName();
    }
}

const SocketCANInterface* SocketCANInterfaceGroup::getInterface(std::uint_fast8_t index) const
{
    if (index >= interfaces_.size())
    {
        return nullptr;
    }
    else
    {
        return interfaces_[index].get();
    }
}

libuavcan::Result SocketCANInterfaceGroup::write(std::uint_fast8_t interface_index,
                                                 const InterfaceType::FrameType (&frames)[TxFramesLen],
                                                 std::size_t  frames_len,
                                                 std::size_t& out_frames_written)
{
    if (interface_index >= interfaces_.size())
    {
        return libuavcan::Result::BadArgument;
    }
    else
    {
        return interfaces_[interface_index]->write(frames, frames_len, out_frames_written);
    }
}

libuavcan::Result SocketCANInterfaceGroup::read(std::uint_fast8_t interface_index,
                                                InterfaceType::FrameType (&out_frames)[RxFramesLen],
                                                std::size_t& out_frames_read)
{
    if (interface_index >= interfaces_.size())
    {
        return libuavcan::Result::BadArgument;
    }
    else
    {
        return interfaces_[interface_index]->read(out_frames, out_frames_read);
    }
}

libuavcan::Result SocketCANInterfaceGroup::select(libuavcan::duration::Monotonic timeout, bool ignore_write_available)
{
    short int         events         = POLLIN | POLLPRI;
    const std::size_t interfaces_len = interfaces_.size();

    if (!ignore_write_available)
    {
        events |= POLLOUT;
    }

    pollfds_.reserve(interfaces_len);

    struct ::pollfd* pollfds = pollfds_.data();

    for (size_t i = 0; i < interfaces_len; ++i)
    {
        pollfds[i] = {interfaces_[i]->getSocketDescriptor(), events, 0};
    }

    ::timespec timeout_spec = {timeout.toMicrosecond() / 1000000U, 0};
    timeout_spec.tv_nsec =
        (timeout - libuavcan::duration::Monotonic::fromMicrosecond(timeout_spec.tv_sec * 1000000U)).toMicrosecond();

    const int result = ::ppoll(pollfds, interfaces_len, &timeout_spec, nullptr);

    if (0 == result)
    {
        return libuavcan::Result::SuccessTimeout;
    }

    if (0 > result)
    {
        return libuavcan::Result::Failure;
    }

    for (size_t i = 0; i < interfaces_len; ++i)
    {
        if (0 != (pollfds[i].revents & (POLLPRI | POLLERR | POLLHUP | POLLNVAL)))
        {
            return libuavcan::Result::SuccessPartial;
        }
    }
    return libuavcan::Result::Success;
}

}  // namespace example
}  // namespace libuavcan

/** @} */  // end of examples group
