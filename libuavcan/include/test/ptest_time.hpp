/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Unit tests of time types and functions.
 */
/** @file */
#ifndef UAVCAN_TEST_TIME_HPP_INCLUDED
#define UAVCAN_TEST_TIME_HPP_INCLUDED

#include "uavcan/uavcan.hpp"
#include "uavcan/time.hpp"
#include "gtest/gtest.h"

namespace uavcan
{
/**
 * @namespace test
 * Contains test fixtures, test helpers, and parameterized tests to help
 * implement libuavcan for a given application.
 */
namespace test
{
/**
 * Test fixture for testing realizations of the uavcan::DurationBase template.
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
    ASSERT_EQ(static_cast<int64_t>(0), instance.toUSec());
}

/**
 * Test that the test type implements the "fromUSec" concept.
 */
TYPED_TEST_P(DurationTest, Concept_fromUSec)
{
    ASSERT_EQ(static_cast<int64_t>(100), TypeParam::fromUSec(100).toUSec());
}

/**
 * Verify that the + operator is saturating for DurationBase instances.
 */
TYPED_TEST_P(DurationTest, SaturatedAdd)
{
    TypeParam instance = TypeParam::fromUSec(std::numeric_limits<int64_t>::max()) + TypeParam::fromUSec(1);
    ASSERT_EQ(std::numeric_limits<int64_t>::max(), instance.toUSec());
}

/**
 * Verify that the - operator is saturating for DurationBase instances.
 */
TYPED_TEST_P(DurationTest, SaturatedSubtract)
{
    TypeParam instance = TypeParam::fromUSec(std::numeric_limits<int64_t>::min()) - TypeParam::fromUSec(1);
    ASSERT_EQ(std::numeric_limits<int64_t>::min(), instance.toUSec());
}

// clang-format off
REGISTER_TYPED_TEST_SUITE_P(DurationTest,
                            DefaultValue,
                            Concept_fromUSec,
                            SaturatedAdd,
                            SaturatedSubtract);

// clang-format on

}  // end namespace test
}  // end namespace uavcan

#endif  // UAVCAN_TEST_TIME_HPP_INCLUDED
