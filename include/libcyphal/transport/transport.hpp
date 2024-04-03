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

/// @brief Declares an abstract Cyphal transport interface.
///
class ITransport : public IRunnable
{
public:
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
    virtual cetl::optional<NodeId> getLocalNodeId() const = 0;

};  // ITransport

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_TRANSPORT_HPP_INCLUDED
