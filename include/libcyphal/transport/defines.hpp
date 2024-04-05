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

struct ProtocolParams final
{
    TransferId  transfer_id_modulo;
    std::size_t mtu_bytes;
    NodeId      max_nodes;

    ProtocolParams(TransferId _transfer_id_modulo, std::size_t _mtu_bytes, NodeId _max_nodes)
        : transfer_id_modulo{_transfer_id_modulo}
        , mtu_bytes{_mtu_bytes}
        , max_nodes{_max_nodes}
    {
    }
};

struct TransferMetadata
{
    TransferId transfer_id;
    TimePoint  timestamp;
    Priority   priority;

    TransferMetadata(TransferId _transfer_id, TimePoint _timestamp, Priority _priority)
        : transfer_id{_transfer_id}
        , timestamp{_timestamp}
        , priority{_priority}
    {
    }
};

struct ServiceTransferMetadata final : TransferMetadata
{
    NodeId remote_node_id;

    ServiceTransferMetadata(TransferId _transfer_id, TimePoint _timestamp, Priority _priority, NodeId _remote_node_id)
        : TransferMetadata{_transfer_id, _timestamp, _priority}
        , remote_node_id{_remote_node_id}
    {
    }
};

// TODO: Maybe have `cetl::byte` polyfill for C++20
using PayloadFragments = cetl::span<cetl::span<uint8_t>>;

struct MessageRxTransfer final
{
    TransferMetadata       metadata;
    cetl::optional<NodeId> publisher_node_id;
    DynamicBuffer          payload;

    MessageRxTransfer(TransferMetadata _metadata, cetl::optional<NodeId> _publisher_node_id, DynamicBuffer _payload)
        : metadata{_metadata}
        , publisher_node_id{_publisher_node_id}
        , payload{std::move(_payload)}
    {
    }
};

struct ServiceRxTransfer final
{
    ServiceTransferMetadata metadata;
    DynamicBuffer           payload;

    ServiceRxTransfer(ServiceTransferMetadata _metadata, DynamicBuffer _payload)
        : metadata{_metadata}
        , payload{std::move(_payload)}
    {
    }
};

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_DEFINES_HPP_INCLUDED
