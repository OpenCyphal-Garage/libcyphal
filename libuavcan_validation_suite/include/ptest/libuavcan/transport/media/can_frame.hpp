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
// +==========================================================================+
// | FrameTest
// +==========================================================================+

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
TYPED_TEST_P(FrameTest, Initialization)
{
    TypeParam instance;
    ASSERT_EQ(0, instance.id);
    ASSERT_EQ(CAN::FrameDLC::CodeForLength0, instance.getDLC());
    for (std::size_t i = 0; i < TypeParam::MTUBytes; ++i)
    {
        ASSERT_EQ(0, instance.data[i]);
    }
}

/**
 * Happy-path initialization with some data.
 */
TYPED_TEST_P(FrameTest, InitWithData)
{
    const std::uint8_t data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    TypeParam          instance(1, data, FrameDLC::CodeForLength5);

    ASSERT_EQ(1, instance.id);
    ASSERT_EQ(CAN::FrameDLC::CodeForLength5, instance.getDLC());
    ASSERT_EQ(5, instance.getDataLength());

    for (std::size_t i = 0; i < instance.getDataLength(); ++i)
    {
        ASSERT_EQ(i, instance.data[i]);
    }
}

/**
 * Initialization with a nullptr as data.
 */
TYPED_TEST_P(FrameTest, InitWithData_NullPtr)
{
    TypeParam instance(1, nullptr, FrameDLC::CodeForLength5);
    ASSERT_EQ(1, instance.id);
    ASSERT_EQ(CAN::FrameDLC::CodeForLength0, instance.getDLC());
    ASSERT_EQ(0, instance.getDataLength());
}

/**
 * Verify that the data length can be set but that it cannot be
 * set greater-than the frame's MTU.
 */
TYPED_TEST_P(FrameTest, SetDataLength)
{
    TypeParam instance;
    for (std::uint_fast8_t i = 0; i < TypeFD::MaxFrameSizeBytes + 1; ++i)
    {
        instance.setDataLength(i);
        if (i <= Type2_0::MaxFrameSizeBytes)
        {
            ASSERT_LE(i, instance.getDataLength());
        }
    }
    constexpr std::uint16_t MTUBytes = TypeParam::MTUBytes;
    ASSERT_GE(MTUBytes, instance.getDataLength());
}

template <std::size_t I, std::size_t LEN, std::uint16_t MTUBytes>
struct GetDLCTestLoop
{
    static_assert(I > 0, "Must start at the end.");
    static_assert(LEN > 0, "Must have non-zero length.");
    static_assert(MTUBytes < LEN, "Set LEN > MTUBytes to cover all conditions.");
    static void test(Frame<MTUBytes>& instance)
    {
        instance.setDataLength(I);
        if (I < MTUBytes)
        {
            ASSERT_EQ(Frame<MTUBytes>::lengthToDlc(I), instance.getDLC());
        }
        else
        {
            ASSERT_EQ(Frame<MTUBytes>::lengthToDlc(MTUBytes), instance.getDLC());
        }
        GetDLCTestLoop<I - 1, LEN, MTUBytes>::test(instance);
    }
};

template <std::size_t LEN, std::uint16_t MTUBytes>
struct GetDLCTestLoop<0, LEN, MTUBytes>
{
    static void test(Frame<MTUBytes>& instance)
    {
        instance.setDataLength(0);
        ASSERT_EQ(FrameDLC::CodeForLength0, instance.getDLC());
    }
};

/**
 * Calls getDLC() for all MTU values up to the FD MTU + 1.
 */
TYPED_TEST_P(FrameTest, GetDLC)
{
    TypeParam instance;
    GetDLCTestLoop<TypeFD::MaxFrameSizeBytes + 1, TypeFD::MaxFrameSizeBytes + 1, TypeParam::MTUBytes>::test(instance);
}

/**
 * Ensure that doing something really terrible results in defined
 * behaviour.
 */
TYPED_TEST_P(FrameTest, DLCToLengthEvil) 
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    ASSERT_EQ(0, TypeParam::dlcToLength((FrameDLC)TypeFD::MaxFrameSizeBytes));
#pragma GCC diagnostic pop
}

/**
 * Verify Frame::priorityHigherThan and the > operator.
 */
TYPED_TEST_P(FrameTest, PriorityHigherThan)
{
    const std::uint8_t fake_data[] = {0};
    TypeParam lhs(0, fake_data, FrameDLC::CodeForLength1);
    TypeParam rhs(1, fake_data, FrameDLC::CodeForLength1);

    ASSERT_TRUE(lhs.priorityHigherThan(rhs));
    ASSERT_FALSE(rhs.priorityHigherThan(lhs));
    ASSERT_FALSE(lhs.priorityHigherThan(lhs));

    ASSERT_TRUE(lhs>rhs);
    ASSERT_FALSE(rhs>lhs);
    ASSERT_FALSE(lhs>lhs);
}

/**
 * Verify Frame::priorityLowerThan and the < operator.
 */
TYPED_TEST_P(FrameTest, PriorityLowerThan)
{
    const std::uint8_t fake_data[] = {0};
    TypeParam lhs(0, fake_data, FrameDLC::CodeForLength1);
    TypeParam rhs(1, fake_data, FrameDLC::CodeForLength1);

    ASSERT_TRUE(rhs.priorityLowerThan(lhs));
    ASSERT_FALSE(lhs.priorityLowerThan(rhs));
    ASSERT_FALSE(lhs.priorityLowerThan(lhs));

    ASSERT_FALSE(lhs<rhs);
    ASSERT_TRUE(rhs<lhs);
    ASSERT_FALSE(lhs<lhs);
}

/**
 * Verify frame equality operator: equal.
 */
TYPED_TEST_P(FrameTest, FrameEqual)
{
    const std::uint8_t fake_data_rhs[] = {0, 1, 2, 3, 5, 6, 7, 8};
    const std::uint8_t fake_data_lhs[] = {0, 1, 2, 3, 5, 6, 7, 8};
    TypeParam lhs(0xFFFFFFFF, fake_data_lhs, FrameDLC::CodeForLength8);
    TypeParam rhs(0x1FFFFFFF, fake_data_rhs, FrameDLC::CodeForLength8);

    ASSERT_EQ(lhs, rhs);
}

/**
 * Verify frame equality operator: not-equal.
 */
TYPED_TEST_P(FrameTest, FrameNotEqual)
{
    const std::uint8_t fake_data_baseline[] = {0, 1, 2, 3, 5, 6, 7, 8};
    const std::uint8_t fake_data_not_equal[] = {0, 1, 2, 3, 5, 6, 7, 9};
    TypeParam baseline(0x1FFFFFFF, fake_data_baseline, FrameDLC::CodeForLength8);
    TypeParam length_different(0x1FFFFFFF, fake_data_baseline, FrameDLC::CodeForLength7);
    TypeParam data_different(0x1FFFFFFF, fake_data_not_equal, FrameDLC::CodeForLength8);
    TypeParam id_different(0x1FFFFFF1, fake_data_baseline, FrameDLC::CodeForLength8);

    ASSERT_NE(baseline, length_different);
    ASSERT_NE(baseline, data_different);
    ASSERT_NE(baseline, id_different);
}

/**
 * Verify frame equality operator: not-equal with flag compare.
 */
TYPED_TEST_P(FrameTest, FrameNotEqualWithFlags)
{
    const std::uint8_t fake_data_rhs[] = {0, 1, 2, 3, 5, 6, 7, 8};
    const std::uint8_t fake_data_lhs[] = {0, 1, 2, 3, 5, 6, 7, 8};
    Frame<TypeParam::MTUBytes, 0x01> lhs(0xFFFFFFFF, fake_data_lhs, FrameDLC::CodeForLength8);
    Frame<TypeParam::MTUBytes, 0x01> rhs(0x1FFFFFFF, fake_data_rhs, FrameDLC::CodeForLength8);

    ASSERT_NE(lhs, rhs);
}

// +--------------------------------------------------------------------------+

REGISTER_TYPED_TEST_SUITE_P(FrameTest,  //
                            Initialization,
                            InitWithData,
                            InitWithData_NullPtr,
                            SetDataLength,
                            GetDLC,
                            DLCToLengthEvil,
                            PriorityHigherThan,
                            PriorityLowerThan,
                            FrameEqual,
                            FrameNotEqual,
                            FrameNotEqualWithFlags);

}  // namespace CAN
}  // namespace media
}  // namespace transport
}  // end namespace libuavcan

#endif  // LIBUAVCAN_LVS_PTEST_TRANSPORT_MEDIA_CAN_FRAME_HPP_INCLUDED
