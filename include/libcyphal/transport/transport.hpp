/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_TRANSPORT_HPP_INCLUDED

#include "errors.hpp"
#include "session/msg_sessions.hpp"
#include "session/svc_sessions.hpp"

namespace libcyphal
{
namespace transport
{

class ITransport : public IRunnable
{
public:
    CETL_NODISCARD virtual cetl::optional<NodeId> getLocalNodeId() const noexcept    = 0;
    CETL_NODISCARD virtual ProtocolParams         getProtocolParams() const noexcept = 0;
    CETL_NODISCARD virtual Expected<UniquePtr<session::IMessageRxSession>, AnyError> makeMessageRxSession(
        session::MessageRxParams params) = 0;
    CETL_NODISCARD virtual Expected<UniquePtr<session::IMessageTxSession>, AnyError> makeMessageTxSession(
        session::MessageTxParams params) = 0;
    CETL_NODISCARD virtual Expected<UniquePtr<session::IRequestRxSession>, AnyError> makeRequestRxSession(
        session::RequestRxParams params) = 0;
    CETL_NODISCARD virtual Expected<UniquePtr<session::IRequestTxSession>, AnyError> makeRequestTxSession(
        session::RequestTxParams params) = 0;
    CETL_NODISCARD virtual Expected<UniquePtr<session::IResponseRxSession>, AnyError> makeResponseRxSession(
        session::ResponseRxParams params) = 0;
    CETL_NODISCARD virtual Expected<UniquePtr<session::IResponseTxSession>, AnyError> makeResponseTxSession(
        session::ResponseTxParams params) = 0;

};  // ITransport

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_TRANSPORT_HPP_INCLUDED
