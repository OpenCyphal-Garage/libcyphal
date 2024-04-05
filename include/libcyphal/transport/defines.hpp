/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_DEFINES_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_DEFINES_HPP_INCLUDED

namespace libcyphal
{
namespace transport
{

/// @brief `NodeId` is a 16-bit unsigned integer that represents a node in a Cyphal network.
///
/// Anonymity is represented by an empty `cetl::optional<NodeId>` (see `cetl::nullopt`).
///
using NodeId = std::uint16_t;

/// @brief `PortId` is a 16-bit unsigned integer that represents a port (subject & service) in a Cyphal network.
///
using PortId = std::uint16_t;

/// @brief `TransferId` is a 64-bit unsigned integer that represents a service transfer (request & response)
/// in a Cyphal network.
///
using TransferId = std::uint64_t;

struct ProtocolParams final
{
    std::uint64_t transfer_id_modulo;
    std::size_t   mtu_bytes;
    NodeId        max_nodes;

};  // ProtocolParams

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_DEFINES_HPP_INCLUDED
