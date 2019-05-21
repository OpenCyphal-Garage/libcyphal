/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Unit tests of time types and functions.
 */
/** @file */
#ifndef LIBUAVCAN_LVS_PTEST_TIME_HPP_INCLUDED
#define LIBUAVCAN_LVS_PTEST_TIME_HPP_INCLUDED

#include "gtest/gtest.h"

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/time.hpp"

namespace libuavcan
{
/**
 * Test fixture for testing realizations of the libuavcan::DurationBase template.
 * See https://github.com/google/googletest/blob/master/googletest/docs/advanced.md for
 * more information about typed gtests.
 *
 * @tparam T    The type for a given test realization. Available as TypeParam
 *              from within a test case.
 */
template <typename T>
class DurationTest : public ::testing::Test
{};

TYPED_TEST_SUITE_P(DurationTest);

// +--------------------------------------------------------------------------+
// | TEST CASES
// +--------------------------------------------------------------------------+

/**
 * Ensure that the default duration is 0.
 */
TYPED_TEST_P(DurationTest, DefaultValue)
{
    TypeParam instance;
    ASSERT_EQ(static_cast<std::int64_t>(0), instance.toMicrosecond());
}

/**
 * Test that the test type implements the "fromMicrosecond" concept.
 */
TYPED_TEST_P(DurationTest, Concept_fromMicrosecond)
{
    ASSERT_EQ(static_cast<std::int64_t>(100), TypeParam::fromMicrosecond(100).toMicrosecond());
}

/**
 * Verify that the + operator is saturating for DurationBase instances.
 */
TYPED_TEST_P(DurationTest, SaturatedAdd)
{
    TypeParam instance =
        TypeParam::fromMicrosecond(std::numeric_limits<std::int64_t>::max()) + TypeParam::fromMicrosecond(1);
    ASSERT_EQ(std::numeric_limits<std::int64_t>::max(), instance.toMicrosecond());
}

/**
 * Verify that the - operator is saturating for DurationBase instances.
 */
TYPED_TEST_P(DurationTest, SaturatedSubtract)
{
    TypeParam instance =
        TypeParam::fromMicrosecond(std::numeric_limits<std::int64_t>::min()) - TypeParam::fromMicrosecond(1);
    ASSERT_EQ(std::numeric_limits<std::int64_t>::min(), instance.toMicrosecond());
}

REGISTER_TYPED_TEST_SUITE_P(DurationTest,  //
                            DefaultValue,
                            Concept_fromMicrosecond,
                            SaturatedAdd,
                            SaturatedSubtract);

}  // end namespace libuavcan

#endif  // LIBUAVCAN_LVS_PTEST_TIME_HPP_INCLUDED
