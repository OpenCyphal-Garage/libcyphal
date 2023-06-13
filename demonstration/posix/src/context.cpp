/// @file
/// Implementation of libcyphal::posix::network::Context.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "libcyphal/network/posix/context.hpp"
#include "libcyphal/network/posix/pollers.hpp"
#include "libcyphal/network/posix/sockets.hpp"

namespace libcyphal
{
namespace network
{
namespace posix
{

namespace
{

janky::expected<SocketPointer<network::ip::Socket>> makeIpSocket(
    cetl::pf17::pmr::polymorphic_allocator<network::posix::ip::UDPSocket>& allocator,
    network::ip::Address                                                   local_address,
    network::ip::Socket::Protocol                                          protocol)
{
    if (protocol != network::ip::Socket::Protocol::UDP)
    {
        return janky::unexpected<libcyphal::ResultCode>{ResultCode::NotImplementedError};
    }
    else
    {
        return janky::expected<SocketPointer<network::ip::Socket>>{
            janky::DarkPointer::make_unique<network::ip::Socket, network::posix::ip::UDPSocket>(allocator,
                                                                                                     local_address)};
    }
}

}  // namespace

Context::Context(NetworkMemoryResources&                     memory_resources,
                 std::initializer_list<std::uint32_t>        can_interfaces,
                 std::initializer_list<network::ip::Address> ip_interfaces) noexcept
    : ip_socket_allocator_{memory_resources.ip_socket_memory}
    , ip_interface_enumerator_{ip_interfaces}
    , can_socket_allocator_{memory_resources.can_socket_memory}
    , can_interface_enumerator_{can_interfaces}
    , poll_memory_{memory_resources.poll_memory}
{
    CETL_DEBUG_ASSERT(poll_memory_ != nullptr, "NetworkMemoryResources::poll_memory must not be null.");
}

janky::expected<SocketPointer<network::ip::Socket>> Context::makeBoundUdpMulticastOutputSocket(
    network::ip::Address interface)
{
    auto socket = makeIpSocket(ip_socket_allocator_, interface, network::ip::Socket::Protocol::UDP);

    if (!socket.has_value())
    {
        return socket;
    }

    SocketPointer<network::ip::Socket>& socket_ptr = *socket;

    // Output sockets shall be bound, too, in order to ensure that outgoing packets have the correct
    // source IP address specified. This is particularly important for localhost; an unbound socket
    // there emits all packets from 127.0.0.1 which is certainly not what we need.
    if (!socket_ptr->bind(socket_ptr->getInterfaceAddress(),
                          static_cast<std::uint16_t>(0u)))  //< bind to an ephemeral port
    {
        return janky::unexpected<libcyphal::ResultCode>{ResultCode::NetworkSystemError};
    }

    // Merely binding is not enough for multicast sockets. We also have to configure IP_MULTICAST_IF.
    // https://tldp.org/HOWTO/Multicast-HOWTO-6.html
    // https://stackoverflow.com/a/26988214/1007777
    const Status option_result = socket_ptr->setSocketOption(IPPROTO_IP, IP_MULTICAST_IF, interface);
    if(!option_result)
    {
        return janky::unexpected<libcyphal::ResultCode>{ResultCode::NetworkSystemError};
    }
    unsigned char ttl = 16;  // per [4.3.2.2] of Cyphal 1.0
    if (!socket_ptr->setSocketOption(IPPROTO_IP, IP_MULTICAST_TTL, ttl))
    {
        return janky::unexpected<libcyphal::ResultCode>{ResultCode::NetworkSystemError};
    }
    return socket;
}

janky::expected<SocketPointer<network::ip::Socket>> Context::makeBoundUdpMulticastInputSocket(
    network::ip::Address interface,
    network::ip::Address multicast_address,
    std::uint16_t        multicast_port)
{
    auto socket = makeIpSocket(ip_socket_allocator_, interface, network::ip::Socket::Protocol::UDP);

    if (!socket.has_value())
    {
        return socket;
    }

    SocketPointer<network::ip::Socket>& socket_ptr = *socket;

    // Allow other applications to use the same Cyphal port as well.
    // These options shall be set before the socket is bound.
    // https://stackoverflow.com/questions/14388706/how-do-so-reuseaddr-and-so-reuseport-differ/14388707#14388707
    if (!socket_ptr->setSocketOption(SOL_SOCKET, SO_REUSEADDR, 1U))
    {
        return janky::unexpected<libcyphal::ResultCode>{ResultCode::NetworkSystemError};
    }

    // This is expected to be useful for unicast inputs only.
    // https://stackoverflow.com/a/14388707/1007777
    if (!socket_ptr->setSocketOption(SOL_SOCKET, SO_REUSEPORT, 1U))
    {
        return janky::unexpected<libcyphal::ResultCode>{ResultCode::NetworkSystemError};
    }

    // Binding to the multicast group address is necessary on GNU/Linux: https://habr.com/ru/post/141021/
    if (!socket_ptr->bind(multicast_address, multicast_port))
    {
        return janky::unexpected<libcyphal::ResultCode>{ResultCode::NetworkSystemError};
    }

    return socket;
}

janky::expected<SocketPointer<std::uint32_t>> Context::makeCANSocket(std::uint32_t)
{
    return janky::expected<SocketPointer<std::uint32_t>>{
        janky::unexpected<libcyphal::ResultCode>{ResultCode::NotImplementedError}};
}

InterfaceEnumerator<network::ip::Address>& Context::enumerateIPInterfaces() noexcept
{
    return ip_interface_enumerator_;
}

InterfaceEnumerator<std::uint32_t>& Context::enumerateCANInterfaces() noexcept
{
    return can_interface_enumerator_;
}

janky::expected<PollPointer> Context::makeReadPoll()
{
    return janky::expected<PollPointer>{
        janky::DarkPointer::make_unique<network::IPoll, network::posix::PosixReadPoller>(
            cetl::pf17::pmr::polymorphic_allocator<network::posix::PosixReadPoller>{poll_memory_})};
}

}  // namespace posix
}  // namespace network
}  // namespace libcyphal
