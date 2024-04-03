/// @file
/// libcyphal common header.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_RUNNABLE_HPP_INCLUDED
#define LIBCYPHAL_RUNNABLE_HPP_INCLUDED

#include "libcyphal/types.hpp"

namespace libcyphal
{

/// @brief Declares an abstract Cyphal runnable interface.
///
/// Runnable objects do work asynchronously but only when `run()` is called.
/// This allows super-loop firmware to separate execution into application work and library work,
/// and for threaded software applications to service the library from dedicated threads.
/// Each object that implements `IRunnable` shall document how when it must be run to achieve certain functionality and
/// timing guarantees.
///
class IRunnable
{
public:
    /// @brief Runs the runnable object.
    virtual void run(TimePoint now) = 0;

protected:
    virtual ~IRunnable() = default;

};  // IRunnable

}  // namespace libcyphal

#endif  // LIBCYPHAL_RUNNABLE_HPP_INCLUDED
