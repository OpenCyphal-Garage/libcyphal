/// @file
/// Defines the Session interface for Transport Layer implementations.
///
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
inline namespace session
{

/// @brief Declares message RX session parameters.
///
struct MessageRxParams final
{
    PortId      subject_id;
    std::size_t extent_bytes;

};  // MessageRxParams

/// @brief Declares message TX session parameters.
///
struct MessageTxParams final
{
    PortId subject_id;

};  // MessageTxParams

/// @brief Declares an abstract Cyphal transport message RX session interface.
///
class IMessageRxSession : public IRunnable
{
public:
    CETL_NODISCARD virtual MessageRxParams getParams() const noexcept             = 0;
    virtual void                           setTransferIdTimeout(Duration timeout) = 0;

};  // IMessageRxSession

/// @brief Declares an abstract Cyphal transport message TX session interface.
///
class IMessageTxSession : public IRunnable
{
public:
    CETL_NODISCARD virtual MessageTxParams getParams() const noexcept = 0;

};  // IMessageTxSession

}  // namespace session
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_SESSION_MSG_SESSIONS_HPP_INCLUDED
