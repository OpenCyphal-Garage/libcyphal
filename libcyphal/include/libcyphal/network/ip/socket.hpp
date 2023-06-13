/// @file
/// Contains the Socket abstraction for IP sockets.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_NETWORK_IP_SOCKET_HPP_INCLUDED
#define LIBCYPHAL_NETWORK_IP_SOCKET_HPP_INCLUDED

#include "libcyphal/libcyphal.hpp"
#include "libcyphal/network/ip/address.hpp"
#include "libcyphal/network/socket.hpp"
#include "libcyphal/janky.hpp"

namespace libcyphal
{
namespace network
{
namespace ip
{

/// IP Socket abstraction.
class Socket : public virtual ISocket
{
public:
    static constexpr janky::PolymorphicTypeId TypeId =
        {0x83, 0xa5, 0x5e, 0xbb, 0x53, 0xff, 0x43, 0x15, 0xab, 0xf8, 0x42, 0x92, 0xa7, 0xf7, 0xd0, 0x1b};

    enum class Protocol : int
    {
        UDP = 1
    };

    virtual Status send(const void* buffer, std::size_t length) noexcept = 0;

    virtual Status receiveFrom(void* buffer, std::size_t& buffer_length, Address& from_address) noexcept = 0;

    /// Bind the socket to a local address.
    /// @param bindto_address The address to bind to.
    /// @return ResultCode::AddressError if the address is not available or is already in use.
    ///         ResultCode::ResourceClosedError if the socket was closed.
    ///         ResultCode::UninitializedError if the socket was not initalized.
    ///         ResultCode::NetworkSystemError for all other errors.
    ///         ResultCode::Success if the socket was bound successfully.
    virtual Status bind(Address bindto_address, janky::optional<std::uint16_t> port) noexcept = 0;

    virtual Address                  getInterfaceAddress() const noexcept = 0;
    virtual janky::optional<Address> getBoundAddress() const noexcept     = 0;

    /// @return ResultCode::AddressError if the remote address was not reachable or is invalid/unsupported.
    ///         ResultCode::ResourceClosedError if the socket was closed.
    ///         ResultCode::UninitializedError if the socket was not initalized.
    ///         ResultCode::ConnectionError for all other errors.
    ///         ResultCode::Success if the socket was bound successfully.
    virtual Status connect(Address remote_address, std::uint16_t remote_port) noexcept = 0;

    virtual Status addMulticastMembership(network::ip::Address multicast_address) noexcept    = 0;
    virtual Status removeMulticastMembership(network::ip::Address multicast_address) noexcept = 0;

    // TODO: use CETL variant when available.
    virtual Status setSocketOption(int level, int option_name, network::ip::Address option_value) noexcept = 0;
    virtual Status setSocketOption(int level, int option_name, unsigned char option_value) noexcept        = 0;

protected:
    /// Only DarkPointer can delete this object through the interface.
    friend class cetl::pmr::PolymorphicDeleter<cetl::pf17::pmr::polymorphic_allocator<Socket>>;
    virtual ~Socket() = default;
};

}  // namespace ip
}  // namespace network
}  // namespace libcyphal

#endif  // LIBCYPHAL_NETWORK_IP_SOCKET_HPP_INCLUDED