/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_EXECUTOR_HPP
#define LIBCYPHAL_EXECUTOR_HPP

namespace libcyphal
{

class IExecutor
{
public:
    IExecutor(const IExecutor&)                = delete;
    IExecutor(IExecutor&&) noexcept            = delete;
    IExecutor& operator=(const IExecutor&)     = delete;
    IExecutor& operator=(IExecutor&&) noexcept = delete;

protected:
    IExecutor()  = default;
    ~IExecutor() = default;

};  // IExecutor

}  // namespace libcyphal

#endif  // LIBCYPHAL_EXECUTOR_HPP
