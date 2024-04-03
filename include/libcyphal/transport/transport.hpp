/// @file
/// Defines the Transport interface for Transport Layer implementations.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_TRANSPORT_HPP_INCLUDED

#include "libcyphal/runnable.hpp"

namespace libcyphal
{
namespace transport
{

/// @brief Declares basic transport capabilities.
///
/// These parameters are defined by the underlying transport specifications.
///
/// Normally, the values should never change for a particular transport instance.
/// This is not a hard guarantee, however.
/// For example, a redundant transport aggregator may return a different set of parameters after
/// the set of aggregated transports is changed (i.e., a transport is added or removed).
///
struct ProtocolParams final
{
    /// @brief The cardinality of the set of distinct transfer-ID values; i.e., the overflow period.
    ///
    /// All high-overhead transports (UDP, Serial, etc.) use a sufficiently large value that will never overflow
    /// in a realistic, practical scenario.
    /// The background and motivation are explained at
    /// https://forum.opencyphal.org/t/alternative-transport-protocols/324.
    /// Example: 32 for CAN, (2**64) for UDP.
    ///
    std::uint64_t transfer_id_modulo;

    /// @brief How many nodes can the transport accommodate in a given network.
    ///
    /// Example: 128 for CAN, 65535 for UDP (0xFFFF is reserved).
    ///
    NodeId max_nodes;

    /// @brief The largest maximum number of payload bytes in a single-frame transfer
    /// for the group of network interfaces used by the transport.
    ///
    /// This number can change on systems where the value is configurable.
    ///
    std::size_t mtu_bytes;

}; // ProtocolParams

/// @brief Declares an abstract Cyphal transport interface.
///
class ITransport : public IRunnable
{
public:
    /// @brief Gets (optional) local node-ID assigned to this transport.
    ///
    /// The node-ID is set once during initialization of the transport,
    /// either explicitly (e.g., CAN) or by deriving the node-ID value from the configuration
    /// of the underlying protocol layers (e.g., UDP/IP).
    ///
    /// If the transport does not have a node-ID, this property has the value of `cetl::nullopt`.
    /// and the transport (and the node that uses it) is said to be in the anonymous mode.
    /// While in the anonymous mode, some transports may choose to operate in a particular regime to facilitate
    /// plug-and-play node-ID allocation (for example, a CAN transport may disable automatic retransmission).
    ///
    /// @return Optional integer representing the local node-ID.
    ///
    virtual cetl::optional<NodeId> getLocalNodeId() const noexcept = 0;

    /// @brief Provides information about the properties of the transport protocol implemented by the instance.
    ///
    /// @return `ProtocolParameters` object if the transport is initialized, otherwise ResultCode::UninitializedError.
    ///
    virtual ProtocolParams getProtocolParams() const noexcept = 0;

};  // ITransport

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_TRANSPORT_HPP_INCLUDED
