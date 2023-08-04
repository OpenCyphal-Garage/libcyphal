/// @file
/// Defines the Transport interface for Transport Layer implementations
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///


#ifndef LIBCYPHAL_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_HPP_INCLUDED

#include <cstdint>

#include "libcyphal/libcyphal.hpp"
#include "libcyphal/transport/session.hpp"
#include "libcyphal/transport/payload_metadata.hpp"

namespace libcyphal
{
namespace transport
{

/// Basic transport capabilities. These parameters are defined by the underlying transport specifications.
///
/// Normally, the values should never change for a particular transport instance.
/// This is not a hard guarantee, however.
/// For example, a redundant transport aggregator may return a different set of parameters after
/// the set of aggregated transports is changed (i.e., a transport is added or removed).
struct ProtocolParameters final
{
    /// The cardinality of the set of distinct transfer-ID values; i.e., the overflow period.
    /// All high-overhead transports (UDP, Serial, etc.) use a sufficiently large value that will never overflow
    /// in a realistic, practical scenario.
    /// The background and motivation are explained at
    /// https://forum.opencyphal.org/t/alternative-transport-protocols/324.
    /// Example: 32 for CAN, (2**64) for UDP.
    std::uint64_t transfer_id_modulo;

    /// How many nodes can the transport accommodate in a given network.
    /// Example: 128 for CAN, 65535 for UDP (0xFFFF is reserved).
    std::uint32_t max_nodes;

    /// The largest maximum number of payload bytes in a single-frame transfer for the group of network interfaces
    /// used by the transport. This number can change on systems where the value is configurable.
    std::size_t mtu_bytes;
};

/// Base class for transport-specific low-level statistical counters.
/// Not to be confused with :class:`pycyphal.transport.SessionStatistics`,
/// which is tracked per-session.
class TransportStatistics
{
public:
    ~TransportStatistics() = default;
    TransportStatistics(const TransportStatistics&) = default;
    TransportStatistics& operator=(const TransportStatistics&) = default;
    TransportStatistics(TransportStatistics&&) = delete;
    TransportStatistics& operator=(TransportStatistics&&) = delete;
protected:
    TransportStatistics() = default;
};


/// An abstract Cyphal transport interface. Please read the module documentation for details.
class ITransport
{
public:
    /// Initializes and verifies all input variables.
    /// @note Must be run before using Transport
    /// @return Status of whether Transport is ready to use or not
    virtual Status initialize() = 0;

    /// Provides information about the properties of the transport protocol implemented by the instance.
    /// See ProtocolParameters.
    ///
    /// @return ProtocolParameters object if the transport is initialized, otherwise ResultCode::UninitializedError.
    virtual ProtocolParameters protocolParameters() const noexcept = 0;

    /// The node-ID is set once during initialization of the transport,
    /// either explicitly (e.g., CAN) or by deriving the node-ID value from the configuration
    /// of the underlying protocol layers (e.g., UDP/IP).
    ///
    /// If the transport does not have a node-ID, this property has the value of None,
    /// and the transport (and the node that uses it) is said to be in the anonymous mode.
    /// While in the anonymous mode, some transports may choose to operate in a particular regime to facilitate
    /// plug-and-play node-ID allocation (for example, a CAN transport may disable automatic retransmission).
    ///
    /// @return Optional integer representing the local node-ID.
    virtual janky::optional<NodeID> localNodeId() const = 0;

    /// Closes all active sessions, underlying media instances, and other resources related to this transport instance.
    ///
    /// After a transport is closed, none of its methods nor dependent objects (such as sessions) can be used.
    /// Methods invoked on a closed transport or any of its dependent objects should yield errors.
    /// Subsequent calls to close() will have no effect.
    ///
    /// Failure to close any of the resources does not prevent the method from closing other resources
    /// (best effort policy).
    /// Related error may be suppressed and logged; the last occurred error may be logged after
    /// all resources are closed if such behavior is considered to be meaningful.
    virtual void close() = 0;

    /// This factory method is the only valid way of constructing input session instances.
    /// Beware that construction and retirement of sessions may be costly.
    ///
    /// The transport will always return the same instance unless there is no session object with the requested
    /// specifier, in which case it will be created and stored internally until closed.
    /// The payload metadata parameter is used only when a new instance is created, ignored otherwise.
    /// Implementations are encouraged to use a covariant return type annotation.
    ///
    /// @param specifier        InputSessionSpecifier object representing the session specifier.
    /// @param payload_metadata PayloadMetadata object representing the payload metadata.
    /// @param out              An InputSession object to populate.
    /// @return If successful, then the out argument will be a valid InputSession object.
    virtual Status getInputSession(InputSessionSpecifier specifier, PayloadMetadata payload_metadata, IInputSession*& out) = 0;

    /// This factory method is the only valid way of constructing output session instances.
    /// Beware that construction and retirement of sessions may be costly.
    ///
    /// The transport will always return the same instance unless there is no session object with the requested
    /// specifier, in which case it will be created and stored internally until closed.
    /// The payload metadata parameter is used only when a new instance is created, ignored otherwise.
    /// Implementations are encouraged to use a covariant return type annotation.
    ///
    /// @param specifier        OutputSessionSpecifier object representing the session specifier.
    /// @param payload_metadata PayloadMetadata object representing the payload metadata.
    /// @param out              An OutputSession object to populate.
    /// @return If successful, then the out argument will be a valid OutputSession object.
    virtual Status getOutputSession(OutputSessionSpecifier specifier, PayloadMetadata payload_metadata, IOutputSession*& out) = 0;

protected:
    ~ITransport() = default;
};

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_HPP_INCLUDED
