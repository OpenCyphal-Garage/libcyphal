/// @file
/// Contains the Socket abstraction.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_NETWORK_SOCKET_HPP_INCLUDED
#define LIBCYPHAL_NETWORK_SOCKET_HPP_INCLUDED

#include "libcyphal/libcyphal.hpp"
#include "libcyphal/janky.hpp"

namespace libcyphal
{
namespace network
{

class ISocket : public virtual janky::IPolymorphicType
{
public:
    static constexpr janky::PolymorphicTypeId TypeId =
        {0x1d, 0xd9, 0xec, 0xe7, 0x0a, 0x7d, 0x42, 0xb1, 0x91, 0xaa, 0xd5, 0x46, 0xe9, 0x0b, 0xc1, 0x78};

    /// Get the current status of this socket.
    /// @return ResultCode::Success if the socket is in a good state, ResultCode::ResourceClosedError if the socket
    ///         is closed but otherwise in a good state, or ResultCode::NetworkSystemError if the socket is in an error
    ///         state. The object should be discarded if the status is ResultCode::NetworkSystemError.
    virtual Status getStatus() const noexcept = 0;

    virtual Status send(const void* buffer, std::size_t length) noexcept = 0;

    virtual Status receive(void* buffer, std::size_t& buffer_length) noexcept = 0;

    virtual Status close() noexcept = 0;

protected:
    virtual ~ISocket() = default;
};

}  // namespace network
}  // namespace libcyphal

#endif  // LIBCYPHAL_NETWORK_SOCKET_HPP_INCLUDED