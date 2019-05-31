/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Unit tests of time types and functions.
 */

#include "gtest/gtest.h"

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/time.hpp"
#include "lvs/time.hpp"

namespace lvs
{

typedef ::testing::Types<libuavcan::MonotonicDuration> MyTypes;

// The trailing comma is required. See https://github.com/google/googletest/issues/1419
INSTANTIATE_TYPED_TEST_SUITE_P(Time, DurationTest, MyTypes, );

}  // namespace lvs
