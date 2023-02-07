/*
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Unit tests of the libcyphal CAN media header.
 */
#include "lvs/lvs.hpp"

#include "libcyphal/libcyphal.hpp"
#include "lvs/media/can_frame.hpp"

namespace lvs
{
namespace media
{
namespace CAN
{
/**
 * Verify the immutable properties of CAN FD
 */
TEST(CANBusTest, TypeFD)
{
    ASSERT_EQ(64U, TypeFD::MaxFrameSizeBytes);
}

/**
 * Verify the immutable properties of CAN 2.0
 */
TEST(CANBusTest, Type2_0)
{
    ASSERT_EQ(8U, Type2_0::MaxFrameSizeBytes);
}

typedef ::testing::Types<Frame<TypeFD::MaxFrameSizeBytes>, Frame<Type2_0::MaxFrameSizeBytes>> MyTypes;

// The trailing comma is required. See https://github.com/google/googletest/issues/1419
INSTANTIATE_TYPED_TEST_SUITE_P(Generic, FrameTest, MyTypes, );

}  // namespace CAN
}  // namespace media
}  // namespace lvs
