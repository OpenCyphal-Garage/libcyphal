/// @file
/// Interface to an object, implemented for a given system, that provides access to networking resources.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
// cSpell: words wlist xlist rlist

#ifndef LIBCYPHAL_NETWORK_CONTEXT_HPP_INCLUDED
#define LIBCYPHAL_NETWORK_CONTEXT_HPP_INCLUDED

#include "libcyphal/libcyphal.hpp"
#include "libcyphal/network/socket.hpp"
#include "libcyphal/network/ip/socket.hpp"
#include "libcyphal/network/ip/address.hpp"
#include "libcyphal/network/can/socket.hpp"
#include "libcyphal/network/InterfaceEnumerator.hpp"
#include "libcyphal/network/poll.hpp"

#include "cetl/pf17/memory_resource.hpp"

namespace libcyphal
{
namespace network
{

template <typename SocketType>
using SocketPointer = cetl::pmr::Factory::unique_ptr_t<cetl::pf17::pmr::polymorphic_allocator<SocketType>>;

using PollPointer = cetl::pmr::Factory::unique_ptr_t<cetl::pf17::pmr::polymorphic_allocator<IPoll>>;

/// An interface to a singleton that provides access to a system's networking
/// resources.
class IContext
{
public:
    // +-----------------------------------------------------------------------+
    // | MEDIA
    // +-----------------------------------------------------------------------+
    virtual InterfaceEnumerator<ip::Address>&   enumerateIPInterfaces() noexcept  = 0;
    virtual InterfaceEnumerator<std::uint32_t>& enumerateCANInterfaces() noexcept = 0;

    // +-----------------------------------------------------------------------+
    // | SOCKETS
    // +-----------------------------------------------------------------------+
    /// Create a new socket that supports IP and the given protocol, setup for
    /// use as a tx socket.
    /// @param interface Libcyphal uses the local IP address as a key for redundant network interfaces.
    /// @return ResultCode::Success if the socket was created successfully.
    virtual janky::expected<SocketPointer<ip::Socket>> makeBoundUdpMulticastOutputSocket(ip::Address interface) = 0;

    /// Create a new socket that supports IP and the given protocol, setup as use
    /// as an rx socket.
    /// @param interface Libcyphal uses the local IP address as a key for redundant network interfaces.
    /// @return ResultCode::Success if the socket was created successfully.
    virtual janky::expected<SocketPointer<ip::Socket>> makeBoundUdpMulticastInputSocket(
        ip::Address   interface,
        ip::Address   multicast_address,
        std::uint16_t multicast_port) = 0;

    // TODO
    virtual janky::expected<SocketPointer<std::uint32_t>> makeCANSocket(std::uint32_t canInterfaceId) = 0;

    // +-----------------------------------------------------------------------+
    // | POLL
    // +-----------------------------------------------------------------------+
    /// On posix platforms this is a wrapper around epoll or poll.
    /// On embedded platforms the semantics must support the documented use cases
    /// but does not need to implement the full POSIX semantics.
    virtual janky::expected<PollPointer> makeReadPoll() = 0;

protected:
    virtual ~IContext() = default;
};
}  // namespace network
}  // namespace libcyphal

#endif  // LIBCYPHAL_NETWORK_CONTEXT_HPP_INCLUDED
