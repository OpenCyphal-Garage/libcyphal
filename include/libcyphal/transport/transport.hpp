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
