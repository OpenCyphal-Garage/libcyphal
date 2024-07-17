/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef EXAMPLE_PLATFORM_POSIX_SINGLE_THREADED_EXECUTOR_HPP
#define EXAMPLE_PLATFORM_POSIX_SINGLE_THREADED_EXECUTOR_HPP

#include "posix_executor_extension.hpp"

#include <libcyphal/platform/single_threaded_executor.hpp>

namespace example
{
namespace platform
{
namespace posix
{

class PosixSingleThreadedExecutor : public libcyphal::platform::SingleThreadedExecutor, public IPosixExecutorExtension
{
public:
    using libcyphal::platform::SingleThreadedExecutor::SingleThreadedExecutor;

private:
    // MARK: - IPosixExecutorExtension

};  // PosixSingleThreadedExecutor

}  // namespace posix
}  // namespace platform
}  // namespace example

#endif  // EXAMPLE_PLATFORM_POSIX_SINGLE_THREADED_EXECUTOR_HPP
