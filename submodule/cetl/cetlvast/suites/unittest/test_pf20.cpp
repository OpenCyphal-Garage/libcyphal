/// @file
/// Unit tests for the behavior of the cetl/pf20/cetlpf.hpp header.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetlvast/helpers.hpp"
#include "cetl/pf20/cetlpf.hpp"

namespace
{

TEST(PF20TestSuite, TestSpanIsSpantastic)
{
    std::uint32_t data[5] = {0, 1, 2, 3, 4};
    std::span<std::uint32_t, 3> always_std(&data[1], &data[4]);
    ASSERT_EQ(1, always_std[0]);
    ASSERT_EQ(3, always_std.back());
}

}  // namespace
