/// @file
/// Contains Socket implementations for POSIX systems.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_NETWORK_POSIX_SOCKETS_HPP_INCLUDED
#define LIBCYPHAL_NETWORK_POSIX_SOCKETS_HPP_INCLUDED

#include "libcyphal/libcyphal.hpp"
#include "libcyphal/network/posix/posix.hpp"
#include "libcyphal/network/ip/socket.hpp"
#include "libcyphal/network/ip/address.hpp"

namespace libcyphal
{
namespace network
{
namespace posix
{

class IPosixSocket : public virtual ISocket
{
public:
    static constexpr janky::PolymorphicTypeId TypeId =
        {0x7d, 0x84, 0x29, 0x41, 0x70, 0x6b, 0x45, 0x12, 0xaf, 0xfd, 0x0e, 0x53, 0xa3, 0x0c, 0xa9, 0x9b};

    virtual int getSocketFd() const noexcept = 0;

protected:
    ~IPosixSocket() = default;
};

namespace ip
{

/// POSIX socket implementation for UDP/IP networking.
class UDPSocket : public network::ip::Socket, virtual IPosixSocket
{
public:
    UDPSocket(network::ip::Address local_address) noexcept;

    virtual ~UDPSocket();

    UDPSocket(const UDPSocket&)            = delete;
    UDPSocket& operator=(const UDPSocket&) = delete;
    UDPSocket(UDPSocket&&)                 = default;
    UDPSocket& operator=(UDPSocket&&)      = default;

    // +-----------------------------------------------------------------------+
    // | janky::IPolymorphicType
    // +-----------------------------------------------------------------------+
    static constexpr janky::PolymorphicTypeId TypeId =
        {0xe4, 0x9c, 0x4d, 0x46, 0x38, 0xdf, 0x42, 0x3c, 0x86, 0x66, 0x1c, 0xd8, 0xab, 0xf1, 0xde, 0x55};

    Status queryType(const janky::PolymorphicTypeId& id, void*& out) noexcept override;

    Status queryType(const janky::PolymorphicTypeId& id, const void*& out) const noexcept override;

    bool isEqual(const janky::IPolymorphicType& right) const noexcept override;

    // +-----------------------------------------------------------------------+
    // | network::ISocket
    // +-----------------------------------------------------------------------+

    Status getStatus() const noexcept override;

    Status send(const void* buffer, size_t length) noexcept override;

    Status receive(void* buffer, std::size_t& buffer_length) noexcept override;

    Status close() noexcept override;

    // +-----------------------------------------------------------------------+
    // | IPosixSocket
    // +-----------------------------------------------------------------------+
    int getSocketFd() const noexcept override;

    // +-----------------------------------------------------------------------+
    // | network::ip::Socket
    // +-----------------------------------------------------------------------+
    Status receiveFrom(void* buffer, std::size_t& buffer_length, network::ip::Address& from_address) noexcept override;
    Status bind(network::ip::Address bindto_address, janky::optional<std::uint16_t> port) noexcept override;

    network::ip::Address                  getInterfaceAddress() const noexcept override;
    janky::optional<network::ip::Address> getBoundAddress() const noexcept override;

    Status connect(network::ip::Address remote_address, std::uint16_t remote_port) noexcept override;

    Status addMulticastMembership(network::ip::Address multicast_address) noexcept override;
    Status removeMulticastMembership(network::ip::Address multicast_address) noexcept override;

    // TODO: use CETL variant when available.
    Status setSocketOption(int level, int option_name, network::ip::Address option_value) noexcept override;
    Status setSocketOption(int level, int option_name, unsigned char option_value) noexcept override;

private:
    Status setSocketOption(int level, int option_name, void* option_value, std::size_t option_value_length) noexcept;

    int                                   socket_fd_;
    network::ip::Address                  local_address_;
    janky::optional<network::ip::Address> bound_address_;
    bool                                  is_closed_;
};

}  // namespace ip
}  // namespace posix
}  // namespace network
}  // namespace libcyphal

#endif  // LIBCYPHAL_NETWORK_POSIX_SOCKETS_HPP_INCLUDED