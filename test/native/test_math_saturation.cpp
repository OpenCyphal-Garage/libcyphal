/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Unit tests of the saturation math utilities in util/math.hpp
 */

#include "uavcan/uavcan.hpp"
#include "uavcan/util/math.hpp"
#include "gtest/gtest.h"

namespace
{
template <typename T>
class SaturatedMathTest : public ::testing::Test
{
};

/**
 * Since we don't enable RTTI we supply this name generator to make these tests more legible.
 */
struct IntTypeNameGenerator
{
    /**
     * Supports the "NameGenerator" concept in gtest.
     *
     * @tparam T The TypeParam of the parameterized test.
     * @param  i The index number of the  TYPED_TEST
     * @return A string which identifies the test case.
     */
    template <typename T>
    static std::string GetName(int i)
    {
        std::stringstream f;
        f << i;
        if (std::numeric_limits<T>::is_signed)
        {
            f << " (signed " << sizeof(T) << ")";
        }
        else
        {
            f << " (unsigned " << sizeof(T) << ")";
        }
        return f.str();
    }
};

using IntTypes = ::testing::Types<bool, uint8_t, int8_t, uint32_t, int32_t, uint64_t, int64_t>;
TYPED_TEST_SUITE(SaturatedMathTest, IntTypes, IntTypeNameGenerator);

// +--------------------------------------------------------------------------+
// | TEST CASES
// +--------------------------------------------------------------------------+

TYPED_TEST(SaturatedMathTest, SaturatingAdd)
{
    const TypeParam a = std::numeric_limits<TypeParam>::max();
    const TypeParam b = 1;
    const TypeParam c = libuavcan::util::saturating_add(a, b);
    ASSERT_EQ(a, c);
}

TYPED_TEST(SaturatedMathTest, SaturatingSub)
{
    const TypeParam a = std::numeric_limits<TypeParam>::min();
    const TypeParam b = 1;
    const TypeParam c = libuavcan::util::saturating_sub(a, b);
    ASSERT_EQ(a, c);
}

TYPED_TEST(SaturatedMathTest, NonSaturatingAdd)
{
    const TypeParam a = std::numeric_limits<TypeParam>::max() - 2;
    const TypeParam b = 1;
    const TypeParam c = libuavcan::util::saturating_add(a, b);
    ASSERT_EQ(static_cast<TypeParam>(a + 1), c);
}

TYPED_TEST(SaturatedMathTest, NonSaturatingSub)
{
    const TypeParam a = std::numeric_limits<TypeParam>::min() + 2;
    const TypeParam b = 1;
    const TypeParam c = libuavcan::util::saturating_sub(a, b);
    ASSERT_EQ(static_cast<TypeParam>(a - 1), c);
}

// +--[ADD::LIMITS]-----------------------------------------------------------+
TYPED_TEST(SaturatedMathTest, SaturatingAddMaxMax)
{
    const TypeParam a = std::numeric_limits<TypeParam>::max();
    const TypeParam b = std::numeric_limits<TypeParam>::max();
    const TypeParam c = libuavcan::util::saturating_add(a, b);
    ASSERT_EQ(std::numeric_limits<TypeParam>::max(), c);
}

TYPED_TEST(SaturatedMathTest, SaturatingAddMinMax)
{
    const TypeParam a = std::numeric_limits<TypeParam>::min();
    const TypeParam b = std::numeric_limits<TypeParam>::max();
    const TypeParam c = libuavcan::util::saturating_add(a, b);
    if (std::numeric_limits<TypeParam>::is_signed)
    {
        ASSERT_EQ(static_cast<TypeParam>(-1), c);
    }
    else
    {
        ASSERT_EQ(std::numeric_limits<TypeParam>::max(), c);
    }
}

TYPED_TEST(SaturatedMathTest, SaturatingAddMinMin)
{
    const TypeParam a = std::numeric_limits<TypeParam>::min();
    const TypeParam b = std::numeric_limits<TypeParam>::min();
    const TypeParam c = libuavcan::util::saturating_add(a, b);
    ASSERT_EQ(std::numeric_limits<TypeParam>::min(), c);
}

TYPED_TEST(SaturatedMathTest, SaturatingAddMaxMin)
{
    const TypeParam a = std::numeric_limits<TypeParam>::max();
    const TypeParam b = std::numeric_limits<TypeParam>::min();
    const TypeParam c = libuavcan::util::saturating_add(a, b);
    if (std::numeric_limits<TypeParam>::is_signed)
    {
        ASSERT_EQ(static_cast<TypeParam>(-1), c);
    }
    else
    {
        ASSERT_EQ(std::numeric_limits<TypeParam>::max(), c);
    }
}

TYPED_TEST(SaturatedMathTest, SaturatingAddMaxMinPlusOne)
{   
    const TypeParam a = std::numeric_limits<TypeParam>::max();
    const TypeParam b = std::numeric_limits<TypeParam>::min() + 1;
    const TypeParam c = libuavcan::util::saturating_add(a, b);
    if (std::numeric_limits<TypeParam>::is_signed)
    {
        ASSERT_EQ(static_cast<TypeParam>(0), c);
    }
    else
    {
        ASSERT_EQ(std::numeric_limits<TypeParam>::max(), c);
    }
}

TYPED_TEST(SaturatedMathTest, SaturatingAddMinPlusOneMin)
{   
    const TypeParam a = std::numeric_limits<TypeParam>::min() + 1;
    const TypeParam b = std::numeric_limits<TypeParam>::min();
    const TypeParam c = libuavcan::util::saturating_add(a, b);
    if (std::numeric_limits<TypeParam>::is_signed)
    {
        ASSERT_EQ(std::numeric_limits<TypeParam>::min(), c);
    }
    else
    {
        ASSERT_EQ(std::numeric_limits<TypeParam>::min() + 1, c);
    }
}

TYPED_TEST(SaturatedMathTest, SaturatingAddMaxToZero)
{   
    const TypeParam a = 0;
    const TypeParam b = std::numeric_limits<TypeParam>::max();
    const TypeParam c = libuavcan::util::saturating_add(a, b);
    ASSERT_EQ(std::numeric_limits<TypeParam>::max(), c);
}

TYPED_TEST(SaturatedMathTest, SaturatingAddZeroToMax)
{   
    const TypeParam a = std::numeric_limits<TypeParam>::max();
    const TypeParam b = 0;
    const TypeParam c = libuavcan::util::saturating_add(a, b);
    ASSERT_EQ(std::numeric_limits<TypeParam>::max(), c);
}

// +--[SUB::LIMITS]-----------------------------------------------------------+

TYPED_TEST(SaturatedMathTest, SaturatingSubMaxMax)
{
    const TypeParam a = std::numeric_limits<TypeParam>::max();
    const TypeParam b = std::numeric_limits<TypeParam>::max();
    const TypeParam c = libuavcan::util::saturating_sub(a, b);
    ASSERT_EQ(static_cast<TypeParam>(0), c);
}

TYPED_TEST(SaturatedMathTest, SaturatingSubMinMax)
{
    const TypeParam a = std::numeric_limits<TypeParam>::min();
    const TypeParam b = std::numeric_limits<TypeParam>::max();
    const TypeParam c = libuavcan::util::saturating_sub(a, b);
    ASSERT_EQ(std::numeric_limits<TypeParam>::min(), c);
}

TYPED_TEST(SaturatedMathTest, SaturatingSubMinMin)
{
    const TypeParam a = std::numeric_limits<TypeParam>::min();
    const TypeParam b = std::numeric_limits<TypeParam>::min();
    const TypeParam c = libuavcan::util::saturating_sub(a, b);
    ASSERT_EQ(static_cast<TypeParam>(0), c);
}

TYPED_TEST(SaturatedMathTest, SaturatingSubMaxMin)
{
    const TypeParam a = std::numeric_limits<TypeParam>::max();
    const TypeParam b = std::numeric_limits<TypeParam>::min();
    const TypeParam c = libuavcan::util::saturating_sub(a, b);
    ASSERT_EQ(std::numeric_limits<TypeParam>::max(), c);
}

TYPED_TEST(SaturatedMathTest, SaturatingSubMaxMinPlusOne)
{   
    const TypeParam a = std::numeric_limits<TypeParam>::max();
    const TypeParam b = std::numeric_limits<TypeParam>::min() + 1;
    const TypeParam c = libuavcan::util::saturating_sub(a, b);
    if (std::numeric_limits<TypeParam>::is_signed)
    {
        ASSERT_EQ(std::numeric_limits<TypeParam>::max(), c);
    }
    else
    {
        ASSERT_EQ(std::numeric_limits<TypeParam>::max() - 1, c);
    }
}

TYPED_TEST(SaturatedMathTest, SaturatingSubMinPlusOneMin)
{   
    const TypeParam a = std::numeric_limits<TypeParam>::min() + 1;
    const TypeParam b = std::numeric_limits<TypeParam>::min();
    const TypeParam c = libuavcan::util::saturating_sub(a, b);
    ASSERT_EQ(static_cast<TypeParam>(1), c);
}

TYPED_TEST(SaturatedMathTest, SaturatingSubMaxFromZero)
{   
    const TypeParam a = 0;
    const TypeParam b = std::numeric_limits<TypeParam>::max();

    const TypeParam c = libuavcan::util::saturating_sub(a, b);
    ASSERT_LE(std::numeric_limits<TypeParam>::min(), c);

    const TypeParam d = libuavcan::util::saturating_sub(c, b);
    ASSERT_EQ(std::numeric_limits<TypeParam>::min(), d);
}

TYPED_TEST(SaturatedMathTest, SaturatingSubZeroFromMax)
{   
    const TypeParam a = std::numeric_limits<TypeParam>::max();
    const TypeParam b = 0;

    const TypeParam c = libuavcan::util::saturating_sub(a, b);
    ASSERT_LE(std::numeric_limits<TypeParam>::max(), c);
}

}  // namespace
