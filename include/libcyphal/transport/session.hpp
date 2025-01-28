/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_SESSION_HPP_INCLUDED

#include "libcyphal/types.hpp"

namespace libcyphal
{
namespace transport
{

/// @brief Defines an abstract interface of a transport layer session.
///
/// A session is a logical connection between nodes in a network.
/// The session is used to send and receive data between the nodes.
/// The session is not responsible for the actual data transfer, but rather for the management of the data transfer.
/// Actual data transfer is done by the transport entity, by means of user provided media and executor,
/// as well as integration with corresponding transport kind (CAN, UDP, etc) lizard library.
///
class ISession
{
public:
    ISession(const ISession&)                = delete;
    ISession(ISession&&) noexcept            = delete;
    ISession& operator=(const ISession&)     = delete;
    ISession& operator=(ISession&&) noexcept = delete;

protected:
    ISession()  = default;
    ~ISession() = default;
};

/// @brief Defines an abstract interface of a transport layer receive (RX) session.
///
/// @see ISession
///
class IRxSession : public ISession
{
public:
    IRxSession(const IRxSession&)                = delete;
    IRxSession(IRxSession&&) noexcept            = delete;
    IRxSession& operator=(const IRxSession&)     = delete;
    IRxSession& operator=(IRxSession&&) noexcept = delete;

    /// @brief Sets the timeout for a transmission.
    ///
    /// Note that b/c of the nature of the Lizard libraries, the timeout is shared
    /// between different sessions of the same kind and port id. This means that
    /// the timeout set for one session will affect all other sessions of the same kind and port id.
    /// For example, two RPC clients (on the same service id) of two different RPC server nodes
    /// will share the same timeout for transfer ids.
    /// See Cyphal specification about transfer-ID timeouts.
    ///
    /// @param timeout - Positive duration for the timeout. The default value is 2 seconds.
    ///                  Zero or negative values are ignored.
    ///
    virtual void setTransferIdTimeout(const Duration timeout) = 0;

protected:
    IRxSession()  = default;
    ~IRxSession() = default;
};

/// @brief Defines an abstract interface of a transport layer transmit (TX) session.
///
/// @see ISession
///
class ITxSession : public ISession
{
public:
    ITxSession(const ITxSession&)                = delete;
    ITxSession(ITxSession&&) noexcept            = delete;
    ITxSession& operator=(const ITxSession&)     = delete;
    ITxSession& operator=(ITxSession&&) noexcept = delete;

protected:
    ITxSession()  = default;
    ~ITxSession() = default;
};

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_SESSION_HPP_INCLUDED
