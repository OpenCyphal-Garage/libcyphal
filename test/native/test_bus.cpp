/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Unit tests of the libuavcan CAN bus headers.
 */

#include "uavcan/uavcan.hpp"
#include "uavcan/bus/can.hpp"
#include "gtest/gtest.h"

/**
 * Verify the immutable properties of CAN FD
 */
TEST(CanBusTest, TypeFd) {
    ASSERT_EQ(64U, uavcan::bus::CAN::TypeFd::max_frame_size_bytes);
}

/**
 * Verify the immutable properties of CAN 2.0
 */
TEST(CanBusTest, Type2_0) {
    ASSERT_EQ(8U, uavcan::bus::CAN::Type2_0::max_frame_size_bytes);
}
