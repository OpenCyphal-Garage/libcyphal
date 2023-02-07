/*
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

namespace libcyphal
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

libcyphal::Result SocketCANInterfaceGroup::write(std::uint_fast8_t interface_index,
                                                 const InterfaceType::FrameType (&frames)[TxFramesLen],
                                                 std::size_t  frames_len,
                                                 std::size_t& out_frames_written)
{
    if (interface_index >= interfaces_.size())
    {
        return libcyphal::Result::BadArgument;
    }
    else
    {
        return interfaces_[interface_index]->write(frames, frames_len, out_frames_written);
    }
}

libcyphal::Result SocketCANInterfaceGroup::read(std::uint_fast8_t interface_index,
                                                InterfaceType::FrameType (&out_frames)[RxFramesLen],
                                                std::size_t& out_frames_read)
{
    if (interface_index >= interfaces_.size())
    {
        return libcyphal::Result::BadArgument;
    }
    else
    {
        return interfaces_[interface_index]->read(out_frames, out_frames_read);
    }
}

libcyphal::Result SocketCANInterfaceGroup::reconfigureFilters(const typename FrameType::Filter* filter_config,
                                                              std::size_t                       filter_config_length)
{
    for(std::uint8_t i = 0; i < interfaces_.size(); ++i)
    {
        const auto result = configureFilters(interfaces_[i]->getSocketDescriptor(), filter_config, filter_config_length);
        if (!result)
        {
            return result;
        }
    }
    return libcyphal::Result::Success;
}

libcyphal::Result SocketCANInterfaceGroup::select(libcyphal::duration::Monotonic timeout, bool ignore_write_available)
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
        (timeout - libcyphal::duration::Monotonic::fromMicrosecond(timeout_spec.tv_sec * 1000000U)).toMicrosecond();

    const int result = ::ppoll(pollfds, interfaces_len, &timeout_spec, nullptr);

    if (0 == result)
    {
        return libcyphal::Result::SuccessTimeout;
    }

    if (0 > result)
    {
        return libcyphal::Result::Failure;
    }

    for (size_t i = 0; i < interfaces_len; ++i)
    {
        if (0 != (pollfds[i].revents & (POLLPRI | POLLERR | POLLHUP | POLLNVAL)))
        {
            return libcyphal::Result::SuccessPartial;
        }
    }
    return libcyphal::Result::Success;
}

libcyphal::Result SocketCANInterfaceGroup::configureFilters(const int                      socket_descriptor,
                                                            const FrameType::Filter* const filter_configs,
                                                            const std::size_t              num_configs)
{
    if (filter_configs == nullptr && num_configs != 0 && num_configs <= CAN_RAW_FILTER_MAX)
    {
        return libcyphal::Result::BadArgument;
    }

    std::vector<::can_filter> socket_filters;

    if (num_configs == 0)
    {
        // The SocketCAN spec indicates that a zero sized filter array can
        // be used to ignore all ingress CAN frames.
        if (0 != setsockopt(socket_descriptor, SOL_CAN_RAW, CAN_RAW_FILTER, nullptr, 0))
        {
            return libcyphal::Result::UnknownInternalError;
        }
        return libcyphal::Result::Success;
    }

    for (unsigned i = 0; i < num_configs; i++)
    {
        const SocketCANInterfaceGroup::FrameType::Filter& fc = filter_configs[i];
        // Use CAN_EFF_FLAG to let the kernel know this is an EFF filter.
        socket_filters.emplace_back(::can_filter{(fc.id & FrameType::MaskExtID) | CAN_EFF_FLAG,  //
                                                 fc.mask | CAN_EFF_FLAG});
    }

    static_assert(sizeof(socklen_t) <= sizeof(std::size_t) &&
                      std::is_signed<socklen_t>::value == std::is_signed<std::size_t>::value,
                  "socklen_t is not of the expected integer type?");

    if (0 != setsockopt(socket_descriptor,
                        SOL_CAN_RAW,
                        CAN_RAW_FILTER,
                        socket_filters.data(),
                        static_cast<socklen_t>(sizeof(can_filter) * socket_filters.size())))
    {
        return libcyphal::Result::UnknownInternalError;
    }

    return libcyphal::Result::Success;
}

}  // namespace example
}  // namespace libcyphal

/** @} */  // end of examples group
