/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/** @file
 * Implement this test for any libcyphal::duration or libcyphal::time objects
 * you create that support the base concepts defined in libcyphal/time.hpp.
 *
 * <h3>Example:</H3>
 * @code
 * #include "lvs/lvs.hpp"
 * #include "lvs/time.hpp"
 *
 * namespace lvs
 * {
 * typedef ::testing::Types<libcyphal::duration::Monotonic,
 *                          libcyphal::time::Monotonic,
 *                          mynamespace::MyDurationType,
 *                          mynamespace::MyTimeType> MyDurationAndTimeTypes;
 *
 * // The trailing comma is required. See https://github.com/google/googletest/issues/1419
 * INSTANTIATE_TYPED_TEST_SUITE_P(Time, DurationOrTimeTest, MyDurationAndTimeTypes, );
 *
 * typedef ::testing::Types<libcyphal::duration::Monotonic, mynamespace::MyDurationType> MyDurationTypes;
 *
 * INSTANTIATE_TYPED_TEST_SUITE_P(Time, DurationTest, MyDurationTypes, );
 *
 * typedef ::testing::Types<libcyphal::time::Monotonic, mynamespace::MyTimeType> MyTimeTypes;
 *
 * INSTANTIATE_TYPED_TEST_SUITE_P(Time, TimeTest, MyTimeTypes, );
 *
 * }  // namespace lvs
 * @endcode
 */
#ifndef LIBCYPHAL_LVS_TIME_HPP_INCLUDED
#define LIBCYPHAL_LVS_TIME_HPP_INCLUDED

#include "gtest/gtest.h"

#include "lvs/lvs.hpp"
#include "libcyphal/time.hpp"

namespace lvs
{
/**
 * Test fixture for testing realizations of the libcyphal::duration::Base or
 * libcyphal::time::Base class. These are tests when the two bases share the same
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
    TypeParam default_ctor;
    ASSERT_EQ(default_value, default_ctor.toMicrosecond());

    // assignment
    TypeParam assignFrom = TypeParam::fromMicrosecond(alt_value);
    default_ctor         = assignFrom;
    ASSERT_EQ(alt_value, default_ctor.toMicrosecond());
    ASSERT_EQ(alt_value, assignFrom.toMicrosecond());

    // copy ctor
    TypeParam copyCtorLhs(default_ctor);
    ASSERT_EQ(alt_value, copyCtorLhs.toMicrosecond());

    // move ctor
    TypeParam moved_into(std::move(default_ctor));
    ASSERT_EQ(alt_value, moved_into.toMicrosecond());
    ASSERT_EQ(default_value, default_ctor.toMicrosecond());

    // move assignment
    TypeParam move_from = TypeParam::fromMicrosecond(alt_value);
    default_ctor        = std::move(move_from);
    ASSERT_EQ(alt_value, default_ctor.toMicrosecond());
    ASSERT_EQ(default_value, move_from.toMicrosecond());

    {
        TypeParam destructed;
        ASSERT_EQ(default_value, destructed.toMicrosecond());
    }
}

/**
 * Test the standard comparison operators supported by duration and time types.
 */
TYPED_TEST_P(DurationOrTimeTest, ComparisonOperators)
{
    TypeParam lhs, rhs;
    ASSERT_TRUE(lhs == rhs);
    ASSERT_FALSE(lhs != rhs);
    ASSERT_FALSE(lhs < rhs);
    ASSERT_FALSE(lhs > rhs);
    ASSERT_TRUE(lhs <= rhs);
    ASSERT_TRUE(lhs >= rhs);
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

/**
 * Ensure that all duration types properly implement the {@code getMaximum()} method.
 */
TYPED_TEST_P(DurationOrTimeTest, GetMaximum)
{
    ASSERT_EQ(TypeParam::fromMicrosecond(std::numeric_limits<typename TypeParam::MicrosecondType>::max()),
              TypeParam::getMaximum());
}

REGISTER_TYPED_TEST_SUITE_P(DurationOrTimeTest,  //
                            ComparisonOperators,
                            DefaultOperations,
                            Concept_fromMicrosecond,
                            SaturatedAdd,
                            GetMaximum,
                            SaturatedSubtract);

/**
 * Test fixture for testing realizations of the libcyphal::duration::Base template.
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
    TypeParam default_ctor;
    ASSERT_EQ(default_value, default_ctor.toMicrosecond());

    // assignment
    TypeParam assignFrom = TypeParam::fromMicrosecond(alt_value);
    default_ctor         = assignFrom;
    ASSERT_EQ(alt_value, default_ctor.toMicrosecond());
    ASSERT_EQ(alt_value, assignFrom.toMicrosecond());

    // copy ctor
    TypeParam copyCtorLhs(default_ctor);
    ASSERT_EQ(alt_value, copyCtorLhs.toMicrosecond());

    // move ctor
    TypeParam moved_into(std::move(default_ctor));
    ASSERT_EQ(alt_value, moved_into.toMicrosecond());
    ASSERT_EQ(default_value, default_ctor.toMicrosecond());

    // move assignment
    TypeParam move_from = TypeParam::fromMicrosecond(alt_value);
    default_ctor        = std::move(move_from);
    ASSERT_EQ(alt_value, default_ctor.toMicrosecond());
    ASSERT_EQ(default_value, move_from.toMicrosecond());

    {
        TypeParam destructed;
        ASSERT_EQ(default_value, destructed.toMicrosecond());
    }
}

/**
 * Verify the ability to get the absolute value of a duration as a duration.
 */
TYPED_TEST_P(DurationTest, AbsoluteValue)
{
    TypeParam instance{TypeParam::fromMicrosecond(-1)};
    ASSERT_EQ(static_cast<typename TypeParam::MicrosecondType>(1), instance.getAbs().toMicrosecond());

    instance = TypeParam::fromMicrosecond(1);
    ASSERT_EQ(static_cast<typename TypeParam::MicrosecondType>(1), instance.getAbs().toMicrosecond());
}

/**
 * Test the standard arithmetic operators for duration types.
 */
TYPED_TEST_P(DurationTest, ArithmeticOperators)
{
    TypeParam lhs{TypeParam::fromMicrosecond(1)};
    TypeParam rhs{TypeParam::fromMicrosecond(1)};
    ASSERT_EQ(TypeParam::fromMicrosecond(2), lhs += rhs);
    lhs += TypeParam::getMaximum();
    ASSERT_EQ(TypeParam::getMaximum(), lhs);
    lhs -= TypeParam::fromMicrosecond(10);
    ASSERT_EQ(TypeParam::getMaximum() - TypeParam::fromMicrosecond(10), lhs);
    ASSERT_EQ(TypeParam::fromMicrosecond(-1), -TypeParam::fromMicrosecond(1));

    // +--[ -MAX ]------------------------------------------------------------+
    ASSERT_EQ(TypeParam::fromMicrosecond(std::numeric_limits<typename TypeParam::MicrosecondType>::min() + 1),
              -TypeParam::getMaximum());

    ASSERT_EQ(TypeParam::fromMicrosecond(std::numeric_limits<typename TypeParam::MicrosecondType>::min() + 2),
              -(TypeParam::getMaximum() - TypeParam::fromMicrosecond(1)));

    // +--[ -MIN ]------------------------------------------------------------+
    // because -MIN == MAX + 1 for twos complement integers we must assume that the minimum duration will
    // remain saturated for -MIN
    ASSERT_EQ(TypeParam::fromMicrosecond(std::numeric_limits<typename TypeParam::MicrosecondType>::max()),
              -TypeParam::fromMicrosecond(std::numeric_limits<typename TypeParam::MicrosecondType>::min()));

    ASSERT_EQ(TypeParam::fromMicrosecond(std::numeric_limits<typename TypeParam::MicrosecondType>::max()),
              -TypeParam::fromMicrosecond(std::numeric_limits<typename TypeParam::MicrosecondType>::min() + 1));

    ASSERT_EQ(TypeParam::fromMicrosecond(std::numeric_limits<typename TypeParam::MicrosecondType>::max() - 1),
              -TypeParam::fromMicrosecond(std::numeric_limits<typename TypeParam::MicrosecondType>::min() + 2));
}

// +--------------------------------------------------------------------------+

REGISTER_TYPED_TEST_SUITE_P(DurationTest,  //
                            AbsoluteValue,
                            ArithmeticOperators,
                            DefaultOperations);

/**
 * Test fixture for testing realizations of the libcyphal::time::Base template.
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
    TypeParam default_ctor;
    ASSERT_EQ(default_value, default_ctor.toMicrosecond());

    // assignment
    TypeParam assignFrom = TypeParam::fromMicrosecond(alt_value);
    default_ctor         = assignFrom;
    ASSERT_EQ(alt_value, default_ctor.toMicrosecond());
    ASSERT_EQ(alt_value, assignFrom.toMicrosecond());

    // copy ctor
    TypeParam copyCtorLhs(default_ctor);
    ASSERT_EQ(alt_value, copyCtorLhs.toMicrosecond());

    // move ctor
    TypeParam moved_into(std::move(default_ctor));
    ASSERT_EQ(alt_value, moved_into.toMicrosecond());
    ASSERT_EQ(default_value, default_ctor.toMicrosecond());

    // move assignment
    TypeParam move_from = TypeParam::fromMicrosecond(alt_value);
    default_ctor        = std::move(move_from);
    ASSERT_EQ(alt_value, default_ctor.toMicrosecond());
    ASSERT_EQ(default_value, move_from.toMicrosecond());

    {
        TypeParam destructed;
        ASSERT_EQ(default_value, destructed.toMicrosecond());
    }
}

/**
 * Test the standard arithmetic operators for duration types.
 */
TYPED_TEST_P(TimeTest, ArithmeticOperators)
{
    ASSERT_EQ(TypeParam::fromMicrosecond(2),
              TypeParam::fromMicrosecond(1) += TypeParam::DurationType::fromMicrosecond(1));

    TypeParam time_plus_max_duration{TypeParam::fromMicrosecond(0) += TypeParam::DurationType::getMaximum()};
    typename TypeParam::DurationType::MicrosecondType t =
        static_cast<typename TypeParam::DurationType::MicrosecondType>(time_plus_max_duration.toMicrosecond());
    ASSERT_EQ(TypeParam::DurationType::getMaximum(), TypeParam::DurationType::fromMicrosecond(t));
    ASSERT_EQ(TypeParam::getMaximum(), TypeParam::getMaximum() += TypeParam::DurationType::getMaximum());
    ASSERT_EQ(TypeParam::fromMicrosecond(0),
              TypeParam::fromMicrosecond(1) -= TypeParam::DurationType::fromMicrosecond(1));
}

REGISTER_TYPED_TEST_SUITE_P(TimeTest,  //
                            ArithmeticOperators,
                            DefaultOperations);
}  // end namespace lvs

#endif  // LIBCYPHAL_LVS_TIME_HPP_INCLUDED
