/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_SESSION_HPP_INCLUDED

#include "libcyphal/runnable.hpp"
#include "defines.hpp"

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

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_SESSION_HPP_INCLUDED
