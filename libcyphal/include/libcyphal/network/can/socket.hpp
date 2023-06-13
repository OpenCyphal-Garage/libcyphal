/// @file
/// Contains the Socket abstraction for Controller Area Network (CAN) sockets.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_NETWORK_CAN_SOCKET_HPP_INCLUDED
#define LIBCYPHAL_NETWORK_CAN_SOCKET_HPP_INCLUDED

#include "libcyphal/libcyphal.hpp"
#include "libcyphal/network/socket.hpp"

namespace libcyphal
{
namespace network
{
namespace can
{

/// Socket-like CAN abstraction.
class Socket : public ISocket
{
public:
    // TODO
protected:
    ~Socket() = default;
};

}  // namespace can
}  // namespace network
}  // namespace libcyphal

#endif  // LIBCYPHAL_NETWORK_CAN_SOCKET_HPP_INCLUDED