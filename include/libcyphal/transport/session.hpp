/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_SESSION_HPP_INCLUDED

#include "libcyphal/runnable.hpp"
#include "libcyphal/types.hpp"

namespace libcyphal
{
namespace transport
{

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

class IRxSession : public ISession
{
public:
    IRxSession(const IRxSession&)                = delete;
    IRxSession(IRxSession&&) noexcept            = delete;
    IRxSession& operator=(const IRxSession&)     = delete;
    IRxSession& operator=(IRxSession&&) noexcept = delete;

    virtual void setTransferIdTimeout(const Duration timeout) = 0;

protected:
    IRxSession()  = default;
    ~IRxSession() = default;
};

class ITxSession : public ISession
{
public:
    ITxSession(const ITxSession&)                = delete;
    ITxSession(ITxSession&&) noexcept            = delete;
    ITxSession& operator=(const ITxSession&)     = delete;
    ITxSession& operator=(ITxSession&&) noexcept = delete;

    /// @brief Sets the timeout for a transmission.
    ///
    /// The value is added to the original transfer timestamp to determine its deadline.
    /// Any transfer that exceeded this deadline would be dropped.
    ///
    /// @param timeout - Positive duration for transmission timeout. Default value is 1 second.
    ///
    virtual void setSendTimeout(const Duration timeout) = 0;

protected:
    ITxSession()  = default;
    ~ITxSession() = default;
};

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_SESSION_HPP_INCLUDED
