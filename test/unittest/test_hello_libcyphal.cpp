/// @file
/// Delete me once you have at least one test built.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "libcyphal/util/math.hpp"
#include "libcyphal/platform/memory.hpp"

namespace
{

TEST(HelloGoogleTest, HelloWorld)
{
    ASSERT_STREQ("Hello World!", "Hello World!");
    libcyphal::platform::memory::PoolAllocator<24, 8> p;
    (void)p.allocate(1);
}

TEST(HelloGoogleTest, Math)
{
    // teo
    ASSERT_EQ(std::numeric_limits<std::uint8_t>::max(),
              libcyphal::util::saturating_add(std::numeric_limits<std::uint8_t>::max(), 1));
}

}  // namespace
