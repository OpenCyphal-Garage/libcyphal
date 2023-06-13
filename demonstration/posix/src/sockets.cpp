/// @file
/// Contains Socket implementations for POSIX systems.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "libcyphal/network/posix/sockets.hpp"

namespace libcyphal
{
namespace network
{
// +---------------------------------------------------------------------------+
// | POLYMORPHIC TYPE ID STORAGE
// |   C++14 requires explicit specification of which translation units the
// | static storage for IPolymorphicType TypeId fields should be allocated
// | within. While C++17 and newer do not require this they do allow it.
// +---------------------------------------------------------------------------+
constexpr janky::PolymorphicTypeId ISocket::TypeId;
constexpr janky::PolymorphicTypeId ip::Socket::TypeId;
namespace posix
{
constexpr janky::PolymorphicTypeId IPosixSocket::TypeId;
namespace ip
{
constexpr janky::PolymorphicTypeId UDPSocket::TypeId;
// +---------------------------------------------------------------------------+

namespace
{
static inline void toSockAddr(network::ip::Address           address,
                              sockaddr&                      inout_sockaddr,
                              janky::optional<std::uint16_t> port = janky::nullopt) noexcept
{
    ::memset(&inout_sockaddr, 0, sizeof(sockaddr));
    inout_sockaddr.sa_family = AF_INET;

    if (port.has_value())
    {
        inout_sockaddr.sa_len = 2;
        *reinterpret_cast<std::uint16_t*>(&inout_sockaddr.sa_data[0]) = htons(*port);
    }
    *reinterpret_cast<long*>(&inout_sockaddr.sa_data[inout_sockaddr.sa_len]) = htonl(address.asInteger());
    inout_sockaddr.sa_len += 4;
}

static inline void toSockAddrIn(network::ip::Address address, sockaddr_in& inout_sockaddr, janky::optional<std::uint16_t> port = janky::nullopt) noexcept
{
    memset(&inout_sockaddr, 0, sizeof(sockaddr_in));
    if (port.has_value())
    {
        inout_sockaddr.sin_port   = htons(*port);
    }
    inout_sockaddr.sin_family = AF_INET;
    inout_sockaddr.sin_addr.s_addr = htonl(address.asInteger());
}

static inline void toSockAddrIn(network::ip::Address address, in_addr& inout_sockaddr) noexcept
{
    memset(&inout_sockaddr, 0, sizeof(in_addr));
    inout_sockaddr.s_addr = htonl(address.asInteger());
}

}  // namespace

UDPSocket::UDPSocket(network::ip::Address local_address) noexcept
    : socket_fd_{::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)}
    , local_address_(local_address)
    , bound_address_{}
    , is_closed_{false}
{
}

UDPSocket::~UDPSocket()
{
    close();
}

int UDPSocket::getSocketFd() const noexcept
{
    return socket_fd_;
}

Status UDPSocket::setSocketOption(int         level,
                                  int         option_name,
                                  void*       option_value,
                                  std::size_t option_value_length) noexcept
{
    if (is_closed_)
    {
        return ResultCode::ResourceClosedError;
    }
    if (socket_fd_ == -1)
    {
        return ResultCode::UninitializedError;
    }
    if (option_value_length > std::numeric_limits<socklen_t>::max())
    {
        return ResultCode::MemoryError;
    }
    if (-1 == setsockopt(socket_fd_, level, option_name, option_value, static_cast<socklen_t>(option_value_length)))
    {
        return makeNetworkStatusWithErrno(ResultCode::NetworkSystemError, errno);
    }
    else
    {
        return ResultCode::Success;
    }
}
// +---------------------------------------------------------------------------+
// | janky::IPolymorphicType
// +---------------------------------------------------------------------------+

Status UDPSocket::queryType(const janky::PolymorphicTypeId& id, void*& out) noexcept
{
    // TODO: provide janky/CETL template that can implement this logic given a list of supported GUIDs.
    if (id == network::ISocket::TypeId)
    {
        out = static_cast<network::ISocket*>(this);
        return ResultCode::Success;
    }
    else if (id == posix::IPosixSocket::TypeId)
    {
        out = static_cast<posix::IPosixSocket*>(this);
        return ResultCode::Success;
    }
    else if (id == network::ip::Socket::TypeId)
    {
        out = static_cast<network::ip::Socket*>(this);
        return ResultCode::Success;
    }
    else if (id == TypeId)
    {
        out = this;
        return ResultCode::Success;
    }
    else
    {
        out = nullptr;
        return ResultCode::LookupError;
    }
}

Status UDPSocket::queryType(const janky::PolymorphicTypeId& id, const void*& out) const noexcept
{
    if (id == network::ISocket::TypeId)
    {
        out = static_cast<const network::ISocket*>(this);
        return ResultCode::Success;
    }
    else if (id == posix::IPosixSocket::TypeId)
    {
        out = static_cast<const posix::IPosixSocket*>(this);
        return ResultCode::Success;
    }
    else if (id == network::ip::Socket::TypeId)
    {
        out = static_cast<const network::ip::Socket*>(this);
        return ResultCode::Success;
    }
    else if (id == TypeId)
    {
        out = this;
        return ResultCode::Success;
    }
    else
    {
        out = nullptr;
        return ResultCode::LookupError;
    }
}

bool UDPSocket::isEqual(const janky::IPolymorphicType& right) const noexcept
{
    const UDPSocket* right_as_udp = janky::polymorphic_type_traits::safe_downcast<const UDPSocket>(right);
    if (nullptr == right_as_udp)
    {
        return false;
    }
    // because this object cannot be copied, only moved, the objects are only equal if they point to the same memory.
    // The means only one UDPSocket instance can hold a given file descriptor for a process space.
    return (this == right_as_udp);
}

// +---------------------------------------------------------------------------+
// | network::ISocket
// +---------------------------------------------------------------------------+
Status UDPSocket::getStatus() const noexcept
{
    if (is_closed_)
    {
        return ResultCode::ResourceClosedError;
    }
    if (socket_fd_ == -1)
    {
        return ResultCode::UninitializedError;
    }
    return ResultCode::Success;
}

Status UDPSocket::send(const void* buffer, size_t length) noexcept
{
    if (is_closed_)
    {
        return ResultCode::ResourceClosedError;
    }
    if (socket_fd_ == -1)
    {
        return ResultCode::UninitializedError;
    }
    if (0 == ::send(socket_fd_, buffer, length, 0))
    {
        return ResultCode::Success;
    }
    else
    {
        return makeNetworkStatusWithErrno(ResultCode::NetworkSystemError, errno);
    }
}

Status UDPSocket::receive(void* buffer, std::size_t& buffer_length) noexcept
{
    network::ip::Address ignored_address;
    return receiveFrom(buffer, buffer_length, ignored_address);
}

Status UDPSocket::close() noexcept
{
    if (is_closed_)
    {
        return ResultCode::ResourceClosedError;
    }
    if (socket_fd_ == -1)
    {
        return ResultCode::UninitializedError;
    }

    if (0 == ::close(socket_fd_))
    {
        socket_fd_ = -1;
        is_closed_ = true;
        return ResultCode::Success;
    }
    else
    {
        return makeNetworkStatusWithErrno(ResultCode::NetworkSystemError, errno);
    }
}

// +---------------------------------------------------------------------------+
// | network::ip::Socket
// +---------------------------------------------------------------------------+

Status UDPSocket::receiveFrom(void* buffer, std::size_t& buffer_length, network::ip::Address& from_address) noexcept
{
    if (is_closed_)
    {
        return ResultCode::ResourceClosedError;
    }
    if (socket_fd_ == -1)
    {
        return ResultCode::UninitializedError;
    }
    sockaddr   interface_addr;
    socklen_t  addr_len   = sizeof(interface_addr);
    ssize_t    recvResult = ::recvfrom(socket_fd_, buffer, buffer_length, 0, &interface_addr, &addr_len);
    if (recvResult >= 0)
    {
        buffer_length           = static_cast<std::size_t>(recvResult);
        std::uint32_t host_addr = ntohl(*reinterpret_cast<std::uint32_t*>(&interface_addr.sa_data[0]));
        from_address            = network::ip::Address{host_addr};
        return ResultCode::Success;
    }
    else
    {
        return makeNetworkStatusWithErrno(ResultCode::NetworkSystemError, errno);
    }
}

Status UDPSocket::bind(network::ip::Address bindto_address, janky::optional<std::uint16_t> port) noexcept
{
    if (is_closed_)
    {
        return ResultCode::ResourceClosedError;
    }
    if (socket_fd_ == -1)
    {
        return ResultCode::UninitializedError;
    }

    sockaddr   interface_addr;
    toSockAddr(bindto_address, interface_addr, port);
    if (-1 == ::bind(socket_fd_, &interface_addr, sizeof(interface_addr)))
    {
        ResultCode result = ResultCode::NetworkSystemError;
        int last_errno = errno;
        if (last_errno == EADDRNOTAVAIL || last_errno == EADDRINUSE)
        {
            result = ResultCode::AddressError;
        }
        else if (last_errno == EINVAL)
        {
            result = ResultCode::ResourceClosedError;
        }
        return makeNetworkStatusWithErrno(result, last_errno);
    }
    else
    {
        return ResultCode::Success;
    }
}

network::ip::Address UDPSocket::getInterfaceAddress() const noexcept
{
    return local_address_;
}

janky::optional<network::ip::Address> UDPSocket::getBoundAddress() const noexcept
{
    return bound_address_;
}

Status UDPSocket::connect(network::ip::Address remote_address, std::uint16_t remote_port) noexcept
{
    if (is_closed_)
    {
        return ResultCode::ResourceClosedError;
    }
    if (socket_fd_ == -1)
    {
        return ResultCode::UninitializedError;
    }

    sockaddr_in remote_address_posix;
    toSockAddrIn(remote_address, remote_address_posix, remote_port);
    if (-1 == ::connect(socket_fd_, reinterpret_cast<sockaddr*>(&remote_address_posix), sizeof(remote_address_posix)))
    {
        ResultCode  result = ResultCode::ConnectionError;
        int last_errno = errno;
        if (last_errno == EADDRNOTAVAIL || last_errno == EAFNOSUPPORT)
        {
            result = ResultCode::AddressError;
        }
        return makeNetworkStatusWithErrno(result, last_errno);
    }
    else
    {
        return ResultCode::Success;
    }
}

Status UDPSocket::setSocketOption(int level, int option_name, network::ip::Address option_value) noexcept
{
    in_addr interface_addr;
    toSockAddrIn(option_value, interface_addr);
    return setSocketOption(level, option_name, &interface_addr, sizeof(sockaddr));
}

Status UDPSocket::setSocketOption(int level, int option_name, unsigned char option_value) noexcept
{
    return setSocketOption(level, option_name, &option_value, sizeof(option_value));
}

Status UDPSocket::addMulticastMembership(network::ip::Address multicast_address) noexcept
{
    ip_mreq two_addrs;
    two_addrs.imr_multiaddr.s_addr = htonl(multicast_address.asInteger());
    two_addrs.imr_interface.s_addr = htonl(getInterfaceAddress().asInteger());
    // Note that using INADDR_ANY in IP_ADD_MEMBERSHIP doesn't actually mean "any",
    // it means "choose one automatically"; see https://tldp.org/HOWTO/Multicast-HOWTO-6.html
    // This is why we have to specify the interface explicitly here.
    return setSocketOption(IPPROTO_IP, IP_ADD_MEMBERSHIP, &two_addrs, sizeof(two_addrs));
}

Status UDPSocket::removeMulticastMembership(network::ip::Address multicast_address) noexcept
{
    ip_mreq two_addrs;
    two_addrs.imr_multiaddr.s_addr = htonl(multicast_address.asInteger());
    two_addrs.imr_interface.s_addr = htonl(getInterfaceAddress().asInteger());
    return setSocketOption(IPPROTO_IP, IP_DROP_MEMBERSHIP, &two_addrs, sizeof(two_addrs));
}

}  // namespace ip
}  // namespace posix
}  // namespace network
}  // namespace libcyphal
