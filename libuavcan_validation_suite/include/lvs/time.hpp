/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Unit tests of time types and functions.
 */
/** @file */
#ifndef LIBUAVCAN_LVS_TIME_HPP_INCLUDED
#define LIBUAVCAN_LVS_TIME_HPP_INCLUDED

#include "gtest/gtest.h"

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/time.hpp"

/**
 * Libuavcan Validation Suite
 * @ref LVSGuide
 */
namespace lvs
{
/**
 * Test fixture for testing realizations of the libuavcan::duration::Base or
 * libuavcan::time::Base class. These are tests when the two bases share the same
 * concepts. For tests where their concepts differ see DurationTest or TimeTest.
 * See https://github.com/google/googletest/blob/master/googletest/docs/advanced.md for
 * more information about typed gtests.
 *
 * @tparam T    The type for a given test realization. Available as TypeParam
 *              from within a test case.
 */
template <typename T>
class DurationOrTimeTest : public ::testing::Test
{};

TYPED_TEST_SUITE_P(DurationOrTimeTest);

// +--------------------------------------------------------------------------+
// | TEST CASES
// +--------------------------------------------------------------------------+

/**
 * Ensure that the default duration is 0.
 */
TYPED_TEST_P(DurationOrTimeTest, DefaultOperations)
{
    const typename TypeParam::MicrosecondType default_value = static_cast<typename TypeParam::MicrosecondType>(0);
    const typename TypeParam::MicrosecondType alt_value     = static_cast<typename TypeParam::MicrosecondType>(2);

    // Per http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#cctor-constructors-assignments-and-destructors
    // default ctor
    TypeParam defaultCtor;
    ASSERT_EQ(default_value, defaultCtor.toMicrosecond());

    // assignment
    TypeParam assignFrom = TypeParam::fromMicrosecond(alt_value);
    defaultCtor          = assignFrom;
    ASSERT_EQ(alt_value, defaultCtor.toMicrosecond());
    ASSERT_EQ(alt_value, assignFrom.toMicrosecond());

    // copy ctor
    TypeParam copyCtorLhs(defaultCtor);
    ASSERT_EQ(alt_value, copyCtorLhs.toMicrosecond());

    // move ctor
    TypeParam movedInto(std::move(defaultCtor));
    ASSERT_EQ(alt_value, movedInto.toMicrosecond());
    ASSERT_EQ(default_value, defaultCtor.toMicrosecond());

    // move assignment
    TypeParam moveFrom = TypeParam::fromMicrosecond(alt_value);
    defaultCtor        = std::move(moveFrom);
    ASSERT_EQ(alt_value, defaultCtor.toMicrosecond());
    ASSERT_EQ(default_value, moveFrom.toMicrosecond());

    {
        TypeParam destructed;
        ASSERT_EQ(default_value, destructed.toMicrosecond());
    }
}

/**
 * Test that the test type implements the "fromMicrosecond" concept.
 */
TYPED_TEST_P(DurationOrTimeTest, Concept_fromMicrosecond)
{
    ASSERT_EQ(static_cast<typename TypeParam::MicrosecondType>(100), TypeParam::fromMicrosecond(100).toMicrosecond());
}

/**
 * Verify that the + operator is saturating for Base instances.
 */
TYPED_TEST_P(DurationOrTimeTest, SaturatedAdd)
{
    TypeParam instance = TypeParam::fromMicrosecond(std::numeric_limits<typename TypeParam::MicrosecondType>::max()) +
                         TypeParam::DurationType::fromMicrosecond(1);
    ASSERT_EQ(std::numeric_limits<typename TypeParam::MicrosecondType>::max(), instance.toMicrosecond());
}

/**
 * Verify that the - operator is saturating for Base instances.
 */
TYPED_TEST_P(DurationOrTimeTest, SaturatedSubtract)
{
    TypeParam instance = TypeParam::fromMicrosecond(std::numeric_limits<typename TypeParam::MicrosecondType>::min()) -
                         TypeParam::DurationType::fromMicrosecond(1);
    ASSERT_EQ(std::numeric_limits<typename TypeParam::MicrosecondType>::min(), instance.toMicrosecond());
}

REGISTER_TYPED_TEST_SUITE_P(DurationOrTimeTest,  //
                            DefaultOperations,
                            Concept_fromMicrosecond,
                            SaturatedAdd,
                            SaturatedSubtract);

/**
 * Test fixture for testing realizations of the libuavcan::duration::Base template.
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
TYPED_TEST_P(DurationTest, DefaultOperations)
{
    const typename TypeParam::MicrosecondType default_value = static_cast<typename TypeParam::MicrosecondType>(0);
    const typename TypeParam::MicrosecondType alt_value     = static_cast<typename TypeParam::MicrosecondType>(2);

    // Per http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#cctor-constructors-assignments-and-destructors
    // default ctor
    TypeParam defaultCtor;
    ASSERT_EQ(default_value, defaultCtor.toMicrosecond());

    // assignment
    TypeParam assignFrom = TypeParam::fromMicrosecond(alt_value);
    defaultCtor          = assignFrom;
    ASSERT_EQ(alt_value, defaultCtor.toMicrosecond());
    ASSERT_EQ(alt_value, assignFrom.toMicrosecond());

    // copy ctor
    TypeParam copyCtorLhs(defaultCtor);
    ASSERT_EQ(alt_value, copyCtorLhs.toMicrosecond());

    // move ctor
    TypeParam movedInto(std::move(defaultCtor));
    ASSERT_EQ(alt_value, movedInto.toMicrosecond());
    ASSERT_EQ(default_value, defaultCtor.toMicrosecond());

    // move assignment
    TypeParam moveFrom = TypeParam::fromMicrosecond(alt_value);
    defaultCtor        = std::move(moveFrom);
    ASSERT_EQ(alt_value, defaultCtor.toMicrosecond());
    ASSERT_EQ(default_value, moveFrom.toMicrosecond());

    {
        TypeParam destructed;
        ASSERT_EQ(default_value, destructed.toMicrosecond());
    }
}

REGISTER_TYPED_TEST_SUITE_P(DurationTest,  //
                            DefaultOperations);

/**
 * Test fixture for testing realizations of the libuavcan::time::Base template.
 * See https://github.com/google/googletest/blob/master/googletest/docs/advanced.md for
 * more information about typed gtests.
 *
 * @tparam T    The type for a given test realization. Available as TypeParam
 *              from within a test case.
 */
template <typename T>
class TimeTest : public ::testing::Test
{};

TYPED_TEST_SUITE_P(TimeTest);

// +--------------------------------------------------------------------------+
// | TEST CASES
// +--------------------------------------------------------------------------+

/**
 * Ensure that the default duration is 0.
 */
TYPED_TEST_P(TimeTest, DefaultOperations)
{
    const typename TypeParam::MicrosecondType default_value = static_cast<typename TypeParam::MicrosecondType>(0);
    const typename TypeParam::MicrosecondType alt_value     = static_cast<typename TypeParam::MicrosecondType>(2);

    // Per http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#cctor-constructors-assignments-and-destructors
    // default ctor
    TypeParam defaultCtor;
    ASSERT_EQ(default_value, defaultCtor.toMicrosecond());

    // assignment
    TypeParam assignFrom = TypeParam::fromMicrosecond(alt_value);
    defaultCtor          = assignFrom;
    ASSERT_EQ(alt_value, defaultCtor.toMicrosecond());
    ASSERT_EQ(alt_value, assignFrom.toMicrosecond());

    // copy ctor
    TypeParam copyCtorLhs(defaultCtor);
    ASSERT_EQ(alt_value, copyCtorLhs.toMicrosecond());

    // move ctor
    TypeParam movedInto(std::move(defaultCtor));
    ASSERT_EQ(alt_value, movedInto.toMicrosecond());
    ASSERT_EQ(default_value, defaultCtor.toMicrosecond());

    // move assignment
    TypeParam moveFrom = TypeParam::fromMicrosecond(alt_value);
    defaultCtor        = std::move(moveFrom);
    ASSERT_EQ(alt_value, defaultCtor.toMicrosecond());
    ASSERT_EQ(default_value, moveFrom.toMicrosecond());

    {
        TypeParam destructed;
        ASSERT_EQ(default_value, destructed.toMicrosecond());
    }
}

REGISTER_TYPED_TEST_SUITE_P(TimeTest,  //
                            DefaultOperations);
}  // end namespace lvs

#endif  // LIBUAVCAN_LVS_TIME_HPP_INCLUDED
