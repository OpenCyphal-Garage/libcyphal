/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_TRANSPORT_HPP_INCLUDED

#include "errors.hpp"
#include "msg_sessions.hpp"
#include "svc_sessions.hpp"
#include "types.hpp"

#include "libcyphal/runnable.hpp"
#include "libcyphal/types.hpp"

#include <cetl/pf17/cetlpf.hpp>

namespace libcyphal
{
namespace transport
{

/// @brief Interface for a transport layer.
///
class ITransport : public IRunnable
{
public:
    /// @brief Gets the protocol parameters.
    ///
    /// @return Almost the same parameters as they were passed to the corresponding transport layer factory.
    ///         The only difference is that the `mtu_bytes` is calculated at run-time as current maximum for
    ///         all media interfaces (see f.e. `can::IMedia::getMtu` method).
    ///
    /// @see can::IMedia::getMtu()
    /// @see udp::IMedia::getMtu()
    ///
    virtual ProtocolParams getProtocolParams() const noexcept = 0;

    /// @brief Gets the local node ID (if any).
    ///
    /// It's optional to have a local node ID set (see anonymous nodes in the Cyphal spec).
    /// Initially (by default) it is not set.
    ///
    /// @return The node ID previously assigned to this transport interface (via `setLocalNodeId`).
    ///         Otherwise it's `nullopt` for an anonymous node.
    ///
    virtual cetl::optional<NodeId> getLocalNodeId() const noexcept = 0;

    /// @brief Sets the local node ID.
    ///
    /// It's only possible to set the local node ID once. Subsequent calls will return an argument error.
    /// The only exception is when the current node ID is the same as the one being set - no operation is performed.
    ///
    /// A concrete transport implementation may have a specific/limited range of valid node IDs. For example,
    /// - an UDP transport may have a range of 0...65534 node ids (see `UDPARD_NODE_ID_MAX` in `udpard.h`)
    /// - a CAN bus transport may have a range of 0...127 node ids (see `CANARD_NODE_ID_MAX` in `canard.h`)
    ///
    /// @param node_id Specific node ID to be assigned to this transport interface.
    /// @return `nullopt` on successful set (or when node ID is the same).
    ///         Otherwise an `ArgumentError` in case of the subsequent calls or ID out of range.
    ///
    virtual cetl::optional<ArgumentError> setLocalNodeId(const NodeId node_id) noexcept = 0;

    /// @brief Makes a message receive (RX) session.
    ///
    /// The RX session must never outlive this transport interface.
    ///
    virtual Expected<UniquePtr<IMessageRxSession>, AnyError> makeMessageRxSession(const MessageRxParams& params) = 0;

    /// @brief Makes a message transmit (TX) session.
    ///
    /// The TX session must never outlive this transport interface.
    ///
    virtual Expected<UniquePtr<IMessageTxSession>, AnyError> makeMessageTxSession(const MessageTxParams& params) = 0;

    /// @brief Makes a service request receive (RX) session.
    ///
    /// The RX session must never outlive this transport interface.
    ///
    virtual Expected<UniquePtr<IRequestRxSession>, AnyError> makeRequestRxSession(const RequestRxParams& params) = 0;

    /// @brief Makes a service request transmit (TX) session.
    ///
    /// The TX session must never outlive this transport interface.
    ///
    virtual Expected<UniquePtr<IRequestTxSession>, AnyError> makeRequestTxSession(const RequestTxParams& params) = 0;

    /// @brief Makes a service response receive (RX) session.
    ///
    /// The RX session must never outlive this transport interface.
    ///
    virtual Expected<UniquePtr<IResponseRxSession>, AnyError> makeResponseRxSession(const ResponseRxParams& params) = 0;

    /// @brief Makes a service response transmit (TX) session.
    ///
    /// The TX session must never outlive this transport interface.
    ///
    virtual Expected<UniquePtr<IResponseTxSession>, AnyError> makeResponseTxSession(const ResponseTxParams& params) = 0;

};  // ITransport

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_TRANSPORT_HPP_INCLUDED
