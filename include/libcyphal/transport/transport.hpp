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
#include <cetl/pmr/function.hpp>
#include <cetl/unbounded_variant.hpp>

#include <cstdint>

namespace libcyphal
{
namespace transport
{

/// @brief Interface for a transport layer.
///
class ITransport : public IRunnable
{
public:
    /// Defines structure for reporting transport errors to the user's handler.
    ///
    /// In addition to the error itself, it provides:
    /// - index of media interface related to this error
    /// - a pointer to the entity that has caused this error
    ///
    struct AnyErrorReport final
    {
        /// Defines high level transport (agnostic) operations which could be done by the transport
        /// processes and its entities, and so could be the source of potentially fatal or transient errors.
        ///
        enum class Operation : std::uint8_t
        {
            /// Pushing message to the TX session.
            TxPush,
            /// Accepting frame for a RX session.
            RxAccept,
            /// Receiving frame from the media interface.
            MediaPop,
            /// Pushing frame to the media interface.
            MediaPush,
            /// Configuring media interface (f.e. applying filters).
            MediaConfig,
        };

        /// Holds any transport error.
        ///
        AnyError error;

        /// Holds the operation that has caused this error.
        ///
        /// Could be used as a hint for the user's handler to understand the context and the `culprit` of the error.
        ///
        Operation operation;

        /// Holds index of media interface that has related to this error.
        ///
        /// This index is the same as the index of the (not `nullptr`!) media interface
        /// pointer in the `media` span argument used at the `makeTransport()` factory method.
        ///
        std::uint8_t media_index;

        /// Holds pointer to an interface of the entity that has caused this error for enhanced context.
        ///
        /// In case of a media entity, it's the media interface pointer (like `can::IMedia*` or `udp::IMedia*`).
        /// In case of a lizard entity, it's the lizard instance pointer (like `struct CanardInstance*` or udp one).
        ///
        cetl::unbounded_variant<sizeof(void*)> culprit;

    };  // AnyErrorReport

    /// @brief Defines signature of a transient error handler.
    ///
    /// If set, this handler is called by the transport layer when a transient media related error occurs during
    /// transport's (or any of its sessions) `run` method. A TX session `send` method may also trigger this handler.
    ///
    /// Note that there is a limited set of things that can be done within this handler, f.e.:
    /// - it's not allowed to call transport's (or its session's) `run` method from within this handler;
    /// - it's not allowed to call a TX session `send` or RX session `receive` methods from within this handler;
    /// - main purpose of the handler:
    ///   - is to log/report/stat the error;
    ///   - potentially modify state of some "culprit" media related component (f.e. reset HW CAN controller);
    ///   - return an optional (maybe different) error back to the transport.
    /// - result error from the handler affects:
    ///   - whether or not other redundant media of this transport will continue to be processed
    ///     as part of this current "problematic" run (see description of return below),
    ///   - propagation of the error up to the original user's call (result of the `run` or `send` methods).
    ///
    /// @param report The error report to be handled. It's made as non-const ref to allow the handler modify it,
    ///               and f.e. reuse original `.error` field value by moving it as is to return result.
    /// @return An optional (maybe different) error back to the transport.
    ///         - If `cetl::nullopt` is returned, the original error (in the `report`) is considered as handled
    ///           and insignificant for the transport. Transport will continue its current process (effectively
    ///           either ignoring such transient failure, or retrying the process later on its next run).
    ///         - If an error is returned, the transport will immediately stop current process, won't process any
    ///           other media (if any), and propagate the returned error to the user (as result of `run` or etc).
    ///
    using TransientErrorHandler =
        cetl::pmr::function<cetl::optional<AnyError>(AnyErrorReport& report), sizeof(void*) * 3>;

    ITransport(const ITransport&)                = delete;
    ITransport(ITransport&&) noexcept            = delete;
    ITransport& operator=(const ITransport&)     = delete;
    ITransport& operator=(ITransport&&) noexcept = delete;

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

    /// Sets new transient error handler.
    ///
    /// If the handler is set, it will be called by the transport layer when a transient media related error occurs.
    /// If the handler is not set (default mode), the transport will ignore such errors and continue
    /// its current process in a "best-effort" manner, namely in assumption that
    /// either other redundant media (if any) will deliver what is needed,
    /// or later retry (aka next `run`) of the operation will resolve the issue.
    /// See \ref TransientErrorHandler for more details.
    ///
    virtual void setTransientErrorHandler(TransientErrorHandler handler) = 0;

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

protected:
    ITransport()  = default;
    ~ITransport() = default;

};  // ITransport

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_TRANSPORT_HPP_INCLUDED
