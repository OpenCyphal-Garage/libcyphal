/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_RUNNABLE_HPP_INCLUDED
#define LIBCYPHAL_RUNNABLE_HPP_INCLUDED

#include "types.hpp"

namespace libcyphal
{

class IRunnable
{
public:
    IRunnable(const IRunnable&)                = delete;
    IRunnable(IRunnable&&) noexcept            = delete;
    IRunnable& operator=(const IRunnable&)     = delete;
    IRunnable& operator=(IRunnable&&) noexcept = delete;

    virtual void run(const TimePoint now) = 0;

protected:
    IRunnable()          = default;
    virtual ~IRunnable() = default;
};

}  // namespace libcyphal

#endif  // LIBCYPHAL_RUNNABLE_HPP_INCLUDED
