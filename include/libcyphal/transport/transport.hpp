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
    CETL_NODISCARD virtual cetl::optional<NodeId> getLocalNodeId() const noexcept    = 0;
    CETL_NODISCARD virtual ProtocolParams         getProtocolParams() const noexcept = 0;

    CETL_NODISCARD virtual Expected<UniquePtr<session::IMessageRxSession>, AnyError> makeMessageRxSession(
        const session::MessageRxParams& params) = 0;
    CETL_NODISCARD virtual Expected<UniquePtr<session::IMessageTxSession>, AnyError> makeMessageTxSession(
        const session::MessageTxParams& params) = 0;
    CETL_NODISCARD virtual Expected<UniquePtr<session::IRequestRxSession>, AnyError> makeRequestRxSession(
        const session::RequestRxParams& params) = 0;
    CETL_NODISCARD virtual Expected<UniquePtr<session::IRequestTxSession>, AnyError> makeRequestTxSession(
        const session::RequestTxParams& params) = 0;
    CETL_NODISCARD virtual Expected<UniquePtr<session::IResponseRxSession>, AnyError> makeResponseRxSession(
        const session::ResponseRxParams& params) = 0;
    CETL_NODISCARD virtual Expected<UniquePtr<session::IResponseTxSession>, AnyError> makeResponseTxSession(
        const session::ResponseTxParams& params) = 0;

};  // ITransport

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_TRANSPORT_HPP_INCLUDED
