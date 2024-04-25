/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_TRANSPORT_HPP_INCLUDED

#include "errors.hpp"
#include "msg_sessions.hpp"
#include "svc_sessions.hpp"

namespace libcyphal
{
namespace transport
{

class ITransport : public IRunnable
{
public:
    CETL_NODISCARD virtual ProtocolParams getProtocolParams() const noexcept = 0;

    /// @brief Gets the local node ID (if any).
    ///
    /// It's optional to have a local node ID set (see anonymous nodes in the Cyphal spec).
    /// Initially (by default) it is not set.
    ///
    /// @return The node ID previously assigned to this transport interface (via `setLocalNodeId`).
    ///         Otherwise it's `nullopt` for an anonymous node.
    ///
    CETL_NODISCARD virtual cetl::optional<NodeId> getLocalNodeId() const noexcept = 0;

    /// @brief Sets the local node ID.
    ///
    /// It's only possible to set the local node ID once. Subsequent calls will return an argument error.
    /// A concrete transport implementation may have a specific/limited range of valid node IDs. For example,
    /// - an UDP transport may have a range of 0...65534 node ids (see `UDPARD_NODE_ID_MAX` in `udpard.h`)
    /// - a CAN bus transport may have a range of 0...127 node ids (see `CANARD_NODE_ID_MAX` in `canard.h`)
    ///
    /// @param node_id Specific node ID assigned to this transport interface.
    /// @return `nullopt` on success; otherwise an `ArgumentError` in case of the subsequent calls or ID out of range.
    ///
    CETL_NODISCARD virtual cetl::optional<ArgumentError> setLocalNodeId(const NodeId node_id) noexcept = 0;

    CETL_NODISCARD virtual Expected<UniquePtr<IMessageRxSession>, AnyError> makeMessageRxSession(
        const MessageRxParams& params) = 0;
    CETL_NODISCARD virtual Expected<UniquePtr<IMessageTxSession>, AnyError> makeMessageTxSession(
        const MessageTxParams& params) = 0;
    CETL_NODISCARD virtual Expected<UniquePtr<IRequestRxSession>, AnyError> makeRequestRxSession(
        const RequestRxParams& params) = 0;
    CETL_NODISCARD virtual Expected<UniquePtr<IRequestTxSession>, AnyError> makeRequestTxSession(
        const RequestTxParams& params) = 0;
    CETL_NODISCARD virtual Expected<UniquePtr<IResponseRxSession>, AnyError> makeResponseRxSession(
        const ResponseRxParams& params) = 0;
    CETL_NODISCARD virtual Expected<UniquePtr<IResponseTxSession>, AnyError> makeResponseTxSession(
        const ResponseTxParams& params) = 0;

};  // ITransport

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_TRANSPORT_HPP_INCLUDED
