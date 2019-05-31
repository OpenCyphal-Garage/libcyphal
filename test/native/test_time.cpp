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

typedef ::testing::Types<libuavcan::duration::Monotonic, libuavcan::time::Monotonic> MyDurationAndTimeTypes;

// The trailing comma is required. See https://github.com/google/googletest/issues/1419
INSTANTIATE_TYPED_TEST_SUITE_P(Time, DurationOrTimeTest, MyDurationAndTimeTypes, );


typedef ::testing::Types<libuavcan::duration::Monotonic> MyDurationTypes;

INSTANTIATE_TYPED_TEST_SUITE_P(Time, DurationTest, MyDurationTypes, );


typedef ::testing::Types<libuavcan::time::Monotonic> MyTimeTypes;

INSTANTIATE_TYPED_TEST_SUITE_P(Time, TimeTest, MyTimeTypes, );

}  // namespace lvs
