/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_EXECUTOR_MOCK_HPP_INCLUDED
#define LIBCYPHAL_EXECUTOR_MOCK_HPP_INCLUDED

#include <libcyphal/executor.hpp>

#include <gmock/gmock.h>

namespace libcyphal
{

class ExecutorMock : public IExecutor
{
public:
    ExecutorMock()          = default;
    virtual ~ExecutorMock() = default;
};

}  // namespace libcyphal

#endif  // LIBCYPHAL_EXECUTOR_MOCK_HPP_INCLUDED
