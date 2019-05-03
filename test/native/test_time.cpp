/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Unit tests of time types and functions.
 */

#include "gtest/gtest.h"

#include "uavcan/uavcan.hpp"
#include "uavcan/time.hpp"
#include "test/ptest_time.hpp"

namespace libuavcan
{
namespace test
{

typedef ::testing::Types<libuavcan::MonotonicDuration> MyTypes;

// The trailing comma is required. See https://github.com/google/googletest/issues/1419
INSTANTIATE_TYPED_TEST_SUITE_P(Time, DurationTest, MyTypes,);

}
}
