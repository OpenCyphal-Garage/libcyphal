/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_TRANSPORT_HPP_INCLUDED

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

};  // ITransport

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_TRANSPORT_HPP_INCLUDED
