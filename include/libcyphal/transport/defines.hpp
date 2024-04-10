/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_DEFINES_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_DEFINES_HPP_INCLUDED

#include "dynamic_buffer.hpp"

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

enum class Priority
{

    Exceptional = 0,
    Immediate   = 1,
    Fast        = 2,
    High        = 3,
    Nominal     = 4,  ///< Nominal priority level should be the default.
    Low         = 5,
    Slow        = 6,
    Optional    = 7,
};

struct ProtocolParams final
{
    TransferId  transfer_id_modulo;
    std::size_t mtu_bytes;
    NodeId      max_nodes;
};

struct TransferMetadata
{
    TransferId transfer_id;
    TimePoint  timestamp;
    Priority   priority;
};

struct MessageTransferMetadata final : TransferMetadata
{
    cetl::optional<NodeId> publisher_node_id;
};

struct ServiceTransferMetadata final : TransferMetadata
{
    NodeId remote_node_id;
};

/// @brief Defines a span of immutable fragments of payload.
using PayloadFragments = cetl::span<const cetl::span<const cetl::byte>>;

struct MessageRxTransfer final
{
    MessageTransferMetadata metadata;
    DynamicBuffer           payload;
};

struct ServiceRxTransfer final
{
    ServiceTransferMetadata metadata;
    DynamicBuffer           payload;
};

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_DEFINES_HPP_INCLUDED
