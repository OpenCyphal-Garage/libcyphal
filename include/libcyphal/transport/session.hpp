/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_SESSION_HPP_INCLUDED

#include "libcyphal/runnable.hpp"
#include "types.hpp"

namespace libcyphal
{
namespace transport
{

class ISession : public IRunnable
{
public:
};

class IRxSession : public ISession
{
public:
    virtual void setTransferIdTimeout(const Duration timeout) = 0;
};

class ITxSession : public ISession
{
public:
    /// @brief Sets the timeout for a transmission.
    ///
    /// The value is added to the original transfer timestamp to determine its deadline.
    /// Any transfer that exceeded this deadline would be dropped by the transport.
    ///
    /// @param timeout - Positive duration for transmission timeout. Default value is 1 second.
    ///
    virtual void setSendTimeout(const Duration timeout) = 0;
};

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_SESSION_HPP_INCLUDED
