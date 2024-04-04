/// @file
/// Defines the Session interface for Transport Layer implementations.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_SESSION_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_SESSION_SESSION_HPP_INCLUDED

#include "libcyphal/runnable.hpp"

namespace libcyphal
{
namespace transport
{
inline namespace session
{

/// @brief Declares an abstract Cyphal transport session interface.
///
class ISession : public IRunnable
{
public:
};  // ISession

}  // namespace session
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_SESSION_SESSION_HPP_INCLUDED
