/// @file
/// Helpers and common definitions for implementing the Network layer on top of POSIX systems.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_NETWORK_POSIX_HPP_INCLUDED
#define LIBCYPHAL_NETWORK_POSIX_HPP_INCLUDED

#include "libcyphal/libcyphal.hpp"

#include <limits>

// +---------------------------------------------------------------------------+
// | POSIX HEADERS
// +---------------------------------------------------------------------------+
#include <sys/poll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/errno.h>
#include <unistd.h>
// +---------------------------------------------------------------------------+

namespace libcyphal
{
namespace network
{
namespace posix
{

/// If a Status is marked as FlagsLayer::Network then set this bit to indicate the id is a saturated errno value.
/// See errnoAsId for conversion rules.
constexpr std::uint8_t IdIsErrno = 0x1;

/// Standard conversion of a posix errno as a std::uint16_t value. The value is saturated such that
/// std::numeric_limits<std::int16_t>::max() and std::numeric_limits<std::int16_t>::min() should be considered "was
/// clamped" values rather than any specific errno.
constexpr std::uint16_t errnoAsId(int last_errno)
{
    if (last_errno > std::numeric_limits<std::int16_t>::max())
    {
        last_errno = std::numeric_limits<std::int16_t>::max();
    }
    else if (last_errno < std::numeric_limits<std::int16_t>::min())
    {
        last_errno = std::numeric_limits<std::int16_t>::min();
    }
    return static_cast<std::uint16_t>(static_cast<std::int16_t>(last_errno));
}

inline Status makeNetworkStatusWithErrno(ResultCode result, int last_errno)
{
    return Status{result, FlagsLayer::Network, IdIsErrno, errnoAsId(last_errno)};
}

/// Recovers a posix errno from a std::uint16_t value. The value is saturated such that
/// std::numeric_limits<std::int16_t>::max() and std::numeric_limits<std::int16_t>::min() should be considered "was
/// clamped" values rather than any specific errno.
constexpr int errnoFromId(std::uint16_t id)
{
    return static_cast<int>(static_cast<std::int16_t>(id));
}

constexpr bool statusHasErrno(const Status& status)
{
    return isLayerStatus(FlagsLayer::Network, status.flags) && (getFlags(status.flags) & IdIsErrno);
}

}  // namespace posix
}  // namespace network
}  // namespace libcyphal

#endif  // LIBCYPHAL_NETWORK_POSIX_HPP_INCLUDED
