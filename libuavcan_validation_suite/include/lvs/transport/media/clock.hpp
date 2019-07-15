/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/** @file
 * Parameterized unit test for the clock interface.
 */
#ifndef LIBUAVCAN_LVS_TRANSPORT_MEDIA_CLOCK_HPP_INCLUDED
#define LIBUAVCAN_LVS_TRANSPORT_MEDIA_CLOCK_HPP_INCLUDED

#include "gtest/gtest.h"

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/media/clock.hpp"

namespace lvs
{
namespace transport
{
namespace media
{
// +==========================================================================+
// | ClockTest
// +==========================================================================+

/**
 * Test fixture for testing realizations of the libuavcan::media::IClock interface.
 * See https://github.com/google/googletest/blob/master/googletest/docs/advanced.md for
 * more information about typed gtests.
 *
 * @tparam T    The type for a given test realization. Available as TypeParam
 *              from within a test case.
 */
template <typename T>
class ClockTest : public ::testing::Test
{};

TYPED_TEST_SUITE_P(ClockTest);

// +--------------------------------------------------------------------------+
// | TEST CASES
// +--------------------------------------------------------------------------+

/**
 * Ensure that the default duration is 0.
 */
TYPED_TEST_P(ClockTest, Foo)
{
    TypeParam instance;
    ASSERT_EQ(0U, instance.id);
    ASSERT_EQ(CAN::FrameDLC::CodeForLength0, instance.getDLC());
    for (std::size_t i = 0; i < TypeParam::MTUBytes; ++i)
    {
        ASSERT_EQ(0U, instance.data[i]);
    }
}


// +--------------------------------------------------------------------------+

REGISTER_TYPED_TEST_SUITE_P(ClockTest,  //
                            Foo);

}  // namespace media
}  // namespace transport
}  // end namespace lvs

#endif  // LIBUAVCAN_LVS_TRANSPORT_MEDIA_CLOCK_HPP_INCLUDED
