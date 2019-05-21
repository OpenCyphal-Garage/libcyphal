/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Parameterized unit test for the CAN Frame template.
 */
/** @file */
#ifndef LIBUAVCAN_LVS_PTEST_TRANSPORT_MEDIA_CAN_FRAME_HPP_INCLUDED
#define LIBUAVCAN_LVS_PTEST_TRANSPORT_MEDIA_CAN_FRAME_HPP_INCLUDED

#include "gtest/gtest.h"

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/transport/media/can.hpp"

namespace libuavcan
{
namespace transport
{
namespace media
{
namespace CAN
{
/**
 * Test fixture for testing realizations of the libuavcan::transport::media::CAN::Frame template.
 * See https://github.com/google/googletest/blob/master/googletest/docs/advanced.md for
 * more information about typed gtests.
 *
 * @tparam T    The type for a given test realization. Available as TypeParam
 *              from within a test case.
 */
template <typename T>
class FrameTest : public ::testing::Test
{};

TYPED_TEST_SUITE_P(FrameTest);

// +--------------------------------------------------------------------------+
// | TEST CASES
// +--------------------------------------------------------------------------+

/**
 * Ensure that the default duration is 0.
 */
TYPED_TEST_P(FrameTest, InitialSize)
{
    TypeParam instance;
    ASSERT_EQ(CAN::FrameDLC::CodeForLength0, instance.getDLC());
}

TYPED_TEST_P(FrameTest, SetDataLength)
{
    TypeParam instance;
    for(std::uint_fast8_t i = 0; i < 66; ++i)
    {
        instance.setDataLength(i);
        if (i <= 8)
        {
            ASSERT_LE(i, instance.getDataLength());
        }
    }
    constexpr std::uint16_t MtuBytes = TypeParam::MtuBytes;
    ASSERT_GE(MtuBytes, instance.getDataLength());
}

REGISTER_TYPED_TEST_SUITE_P(FrameTest,  //
                            InitialSize,
                            SetDataLength);

}  // namespace CAN
}  // namespace media
}  // namespace transport
}  // end namespace libuavcan

#endif  // LIBUAVCAN_LVS_PTEST_TRANSPORT_MEDIA_CAN_FRAME_HPP_INCLUDED
