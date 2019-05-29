/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Unit tests of the libuavcan CAN media header.
 */
#include "gtest/gtest.h"

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/transport/media/can.hpp"
#include "ptest/libuavcan/transport/media/can_frame.hpp"

namespace libuavcan
{
namespace transport
{
namespace media
{
namespace CAN
{
/**
 * Verify the immutable properties of CAN FD
 */
TEST(CanBusTest, TypeFD)
{
    ASSERT_EQ(64U, TypeFD::MaxFrameSizeBytes);
}

/**
 * Verify the immutable properties of CAN 2.0
 */
TEST(CanBusTest, Type2_0)
{
    ASSERT_EQ(8U, Type2_0::MaxFrameSizeBytes);
}

typedef ::testing::Types<Frame<TypeFD::MaxFrameSizeBytes>, Frame<Type2_0::MaxFrameSizeBytes>> MyTypes;

// The trailing comma is required. See https://github.com/google/googletest/issues/1419
INSTANTIATE_TYPED_TEST_SUITE_P(Generic, FrameTest, MyTypes, );

}  // namespace CAN
}  // namespace media
}  // namespace transport
}  // namespace libuavcan
