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
    virtual ~IRunnable() = default;

    virtual void run(const TimePoint now) = 0;

protected:
    IRunnable() = default;
    IRunnable(const IRunnable&) = default;
    IRunnable(IRunnable&&) = default;

    IRunnable& operator=(const IRunnable&) = default;
    IRunnable& operator=(IRunnable&&)      = default;
};

}  // namespace libcyphal

#endif  // LIBCYPHAL_RUNNABLE_HPP_INCLUDED
