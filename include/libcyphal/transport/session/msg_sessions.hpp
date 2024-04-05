/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_SESSION_MSG_SESSIONS_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_SESSION_MSG_SESSIONS_HPP_INCLUDED

#include "session.hpp"

namespace libcyphal
{
namespace transport
{
namespace session
{

struct MessageRxParams final
{
    std::size_t extent_bytes;
    PortId      subject_id;
};

struct MessageTxParams final
{
    PortId subject_id;
};

class IMessageRxSession : public IRxSession
{
public:
    CETL_NODISCARD virtual MessageRxParams   getParams() const noexcept = 0;
    CETL_NODISCARD virtual MessageRxTransfer receive()                  = 0;
};

class IMessageTxSession : public IRunnable
{
public:
    CETL_NODISCARD virtual MessageTxParams          getParams() const noexcept                     = 0;
    CETL_NODISCARD virtual Expected<void, AnyError> send(const TransferMetadata metadata,
                                                         const PayloadFragments payload_fragments) = 0;
};

}  // namespace session
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_SESSION_MSG_SESSIONS_HPP_INCLUDED
