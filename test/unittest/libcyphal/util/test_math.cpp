/// @file
/// Unit tests of the saturation math utilities in util/math.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
// cSpell: words LHST RHST

#include "libcyphal/libcyphal.hpp"
#include "libcyphal/util/math.hpp"

#include "libcyphal/unittest.hpp"

namespace
{
template <typename LHST, typename RHST>
struct MixedTypeMathParams
{
    int s;
    using LeftHandSideType  = LHST;
    using RightHandSideType = RHST;

    static std::ostream& to_string(std::ostream& out)
    {
        out << "MixedTypeMathParams<";
        if (std::is_signed<LHST>::value)
        {
            out << "signed, ";
        }
        else
        {
            out << "unsigned, ";
        }

        if (std::is_signed<RHST>::value)
        {
            out << "signed";
        }
        else
        {
            out << "unsigned";
        }
        out << ">";
        return out;
    }

    friend void PrintTo(const MixedTypeMathParams<LHST, RHST>& type, std::ostream* out)
    {
        (void) type;
        MixedTypeMathParams<LHST, RHST>::to_string(*out);
    }
};

/**
 * Since we don't enable RTTI we supply this name generator to make these tests more legible.
 */
struct IntTypeNameGenerator
{
    /**
     * Supports the "NameGenerator" concept in gtest.
     *
     * @tparam T The typename TypeParam of the parameterized test.
     * @param  i The index number of the  TYPED_TEST
     * @return A string which identifies the test case.
     */
    template <typename T>
    static std::string GetName(int i)
    {
        std::stringstream out;
        out << i;
        out << " ";
        T::to_string(out);
        return out.str();
    }
};

// +--------------------------------------------------------------------------+

template <typename T>
class SaturatedMathTest : public ::testing::Test
{
    static_assert(sizeof(typename T::LeftHandSideType) == sizeof(typename T::RightHandSideType),
                  "These tests may only be used to compare signed and unsigned versions of the same type.");
    static_assert(
        (std::numeric_limits<typename T::LeftHandSideType>::is_signed ==
         std::numeric_limits<typename T::RightHandSideType>::is_signed) ||
            !std::numeric_limits<typename T::LeftHandSideType>::is_signed,
        "We only support mixed signedness where the LHS is unsigned, for now. We will add in the future if needed.");
};

using IntTypes = ::testing::Types<MixedTypeMathParams<bool, bool>,
                                  MixedTypeMathParams<uint8_t, uint8_t>,
                                  MixedTypeMathParams<int8_t, int8_t>,
                                  MixedTypeMathParams<uint32_t, uint32_t>,
                                  MixedTypeMathParams<int32_t, int32_t>,
                                  MixedTypeMathParams<uint64_t, uint64_t>,
                                  MixedTypeMathParams<int64_t, int64_t>,
                                  MixedTypeMathParams<uint32_t, int32_t>,
                                  MixedTypeMathParams<uint64_t, int64_t>>;

TYPED_TEST_SUITE(SaturatedMathTest, IntTypes, IntTypeNameGenerator);

// +--------------------------------------------------------------------------+
// | TEST CASES
// +--------------------------------------------------------------------------+

TYPED_TEST(SaturatedMathTest, SaturatingAdd)
{
    const typename TypeParam::LeftHandSideType  a = std::numeric_limits<typename TypeParam::LeftHandSideType>::max();
    const typename TypeParam::RightHandSideType b = 1;
    const typename TypeParam::LeftHandSideType  c = libcyphal::util::saturating_add(a, b);
    ASSERT_EQ(a, c);
}

TYPED_TEST(SaturatedMathTest, SaturatingSub)
{
    const typename TypeParam::LeftHandSideType  a = std::numeric_limits<typename TypeParam::LeftHandSideType>::min();
    const typename TypeParam::RightHandSideType b = 1;
    const typename TypeParam::LeftHandSideType  c = libcyphal::util::saturating_sub(a, b);
    ASSERT_EQ(a, c);
}

TYPED_TEST(SaturatedMathTest, NonSaturatingAdd)
{
    const typename TypeParam::LeftHandSideType a = std::numeric_limits<typename TypeParam::LeftHandSideType>::max() - 2;
    const typename TypeParam::RightHandSideType b = 1;
    const typename TypeParam::LeftHandSideType  c = libcyphal::util::saturating_add(a, b);
    ASSERT_EQ(static_cast<typename TypeParam::LeftHandSideType>(a + 1), c);
}

TYPED_TEST(SaturatedMathTest, NonSaturatingSub)
{
    const typename TypeParam::LeftHandSideType a = std::numeric_limits<typename TypeParam::LeftHandSideType>::min() + 2;
    const typename TypeParam::RightHandSideType b = 1;
    const typename TypeParam::LeftHandSideType  c = libcyphal::util::saturating_sub(a, b);
    ASSERT_EQ(static_cast<typename TypeParam::LeftHandSideType>(a - 1), c);
}

// +--[ADD::LIMITS]-----------------------------------------------------------+
TYPED_TEST(SaturatedMathTest, SaturatingAddMaxMax)
{
    const typename TypeParam::LeftHandSideType  a = std::numeric_limits<typename TypeParam::LeftHandSideType>::max();
    const typename TypeParam::RightHandSideType b = std::numeric_limits<typename TypeParam::RightHandSideType>::max();
    const typename TypeParam::LeftHandSideType  c = libcyphal::util::saturating_add(a, b);
    ASSERT_EQ(std::numeric_limits<typename TypeParam::LeftHandSideType>::max(), c);
}

TYPED_TEST(SaturatedMathTest, SaturatingAddMinMax)
{
    const typename TypeParam::LeftHandSideType  a = std::numeric_limits<typename TypeParam::LeftHandSideType>::min();
    const typename TypeParam::RightHandSideType b = std::numeric_limits<typename TypeParam::RightHandSideType>::max();
    const typename TypeParam::LeftHandSideType  c = libcyphal::util::saturating_add(a, b);
    if (std::numeric_limits<typename TypeParam::LeftHandSideType>::is_signed)
    {
        ASSERT_EQ(static_cast<typename TypeParam::LeftHandSideType>(-1), c);
    }
    else if (std::numeric_limits<typename TypeParam::RightHandSideType>::is_signed)
    {
        ASSERT_EQ(static_cast<typename TypeParam::LeftHandSideType>(
                      std::numeric_limits<typename TypeParam::RightHandSideType>::max()),
                  c);
    }
    else
    {
        ASSERT_EQ(std::numeric_limits<typename TypeParam::LeftHandSideType>::max(), c);
    }
}

TYPED_TEST(SaturatedMathTest, SaturatingAddMinMin)
{
    const typename TypeParam::LeftHandSideType  a = std::numeric_limits<typename TypeParam::LeftHandSideType>::min();
    const typename TypeParam::RightHandSideType b = std::numeric_limits<typename TypeParam::RightHandSideType>::min();
    const typename TypeParam::LeftHandSideType  c = libcyphal::util::saturating_add(a, b);
    ASSERT_EQ(std::numeric_limits<typename TypeParam::LeftHandSideType>::min(), c);
}

TYPED_TEST(SaturatedMathTest, SaturatingAddMaxMin)
{
    const typename TypeParam::LeftHandSideType  a = std::numeric_limits<typename TypeParam::LeftHandSideType>::max();
    const typename TypeParam::RightHandSideType b = std::numeric_limits<typename TypeParam::RightHandSideType>::min();
    const typename TypeParam::LeftHandSideType  c = libcyphal::util::saturating_add(a, b);
    if (std::numeric_limits<typename TypeParam::LeftHandSideType>::is_signed)
    {
        ASSERT_EQ(static_cast<typename TypeParam::LeftHandSideType>(-1), c);
    }
    else if (std::numeric_limits<typename TypeParam::RightHandSideType>::is_signed)
    {
        ASSERT_EQ(a - ((std::numeric_limits<typename TypeParam::LeftHandSideType>::max() >> 1U) +
                       static_cast<typename TypeParam::LeftHandSideType>(1)),
                  c);
    }
    else
    {
        ASSERT_EQ(std::numeric_limits<typename TypeParam::LeftHandSideType>::max(), c);
    }
}

TYPED_TEST(SaturatedMathTest, SaturatingAddMaxMinPlusOne)
{
    const typename TypeParam::LeftHandSideType  a = std::numeric_limits<typename TypeParam::LeftHandSideType>::max();
    const typename TypeParam::RightHandSideType b =
        std::numeric_limits<typename TypeParam::RightHandSideType>::min() + 1;
    const typename TypeParam::LeftHandSideType c = libcyphal::util::saturating_add(a, b);
    if (std::numeric_limits<typename TypeParam::LeftHandSideType>::is_signed)
    {
        ASSERT_EQ(static_cast<typename TypeParam::LeftHandSideType>(0), c);
    }
    else if (std::numeric_limits<typename TypeParam::RightHandSideType>::is_signed)
    {
        ASSERT_EQ(a - ((std::numeric_limits<typename TypeParam::LeftHandSideType>::max() >> 1U) +
                       static_cast<typename TypeParam::LeftHandSideType>(2)),
                  c);
    }
    else
    {
        ASSERT_EQ(std::numeric_limits<typename TypeParam::LeftHandSideType>::max(), c);
    }
}

TYPED_TEST(SaturatedMathTest, SaturatingAddMinPlusOneMin)
{
    const typename TypeParam::LeftHandSideType a = std::numeric_limits<typename TypeParam::LeftHandSideType>::min() + 1;
    const typename TypeParam::RightHandSideType b = std::numeric_limits<typename TypeParam::RightHandSideType>::min();
    const typename TypeParam::LeftHandSideType  c = libcyphal::util::saturating_add(a, b);
    if (std::numeric_limits<typename TypeParam::LeftHandSideType>::is_signed)
    {
        ASSERT_EQ(std::numeric_limits<typename TypeParam::LeftHandSideType>::min(), c);
    }
    else if (std::numeric_limits<typename TypeParam::RightHandSideType>::is_signed)
    {
        ASSERT_EQ(static_cast<typename TypeParam::LeftHandSideType>(0), c);
    }
    else
    {
        ASSERT_EQ(std::numeric_limits<typename TypeParam::LeftHandSideType>::min() + 1, c);
    }
}

TYPED_TEST(SaturatedMathTest, SaturatingAddMaxToZero)
{
    const typename TypeParam::LeftHandSideType  a = 0;
    const typename TypeParam::RightHandSideType b = std::numeric_limits<typename TypeParam::RightHandSideType>::max();
    const typename TypeParam::LeftHandSideType  c = libcyphal::util::saturating_add(a, b);
    ASSERT_EQ(static_cast<typename TypeParam::LeftHandSideType>(
                  std::numeric_limits<typename TypeParam::RightHandSideType>::max()),
              c);
}

TYPED_TEST(SaturatedMathTest, SaturatingAddZeroToMax)
{
    const typename TypeParam::LeftHandSideType  a = std::numeric_limits<typename TypeParam::LeftHandSideType>::max();
    const typename TypeParam::RightHandSideType b = 0;
    const typename TypeParam::LeftHandSideType  c = libcyphal::util::saturating_add(a, b);
    ASSERT_EQ(std::numeric_limits<typename TypeParam::LeftHandSideType>::max(), c);
}

// +--[SUB::LIMITS]-----------------------------------------------------------+

TYPED_TEST(SaturatedMathTest, SaturatingSubMaxMax)
{
    const typename TypeParam::LeftHandSideType  a = std::numeric_limits<typename TypeParam::LeftHandSideType>::max();
    const typename TypeParam::RightHandSideType b = std::numeric_limits<typename TypeParam::RightHandSideType>::max();
    const typename TypeParam::LeftHandSideType  c = libcyphal::util::saturating_sub(a, b);
    if (std::numeric_limits<typename TypeParam::LeftHandSideType>::is_signed ==
        std::numeric_limits<typename TypeParam::RightHandSideType>::is_signed)
    {
        ASSERT_EQ(static_cast<typename TypeParam::LeftHandSideType>(0), c);
    }
    else
    {
        ASSERT_EQ(std::numeric_limits<typename TypeParam::LeftHandSideType>::max() -
                      std::numeric_limits<typename TypeParam::RightHandSideType>::max(),
                  c);
    }
}

TYPED_TEST(SaturatedMathTest, SaturatingSubMinMax)
{
    const typename TypeParam::LeftHandSideType  a = std::numeric_limits<typename TypeParam::LeftHandSideType>::min();
    const typename TypeParam::RightHandSideType b = std::numeric_limits<typename TypeParam::RightHandSideType>::max();
    const typename TypeParam::LeftHandSideType  c = libcyphal::util::saturating_sub(a, b);
    ASSERT_EQ(std::numeric_limits<typename TypeParam::LeftHandSideType>::min(), c);
}

TYPED_TEST(SaturatedMathTest, SaturatingSubMinMin)
{
    const typename TypeParam::LeftHandSideType  a = std::numeric_limits<typename TypeParam::LeftHandSideType>::min();
    const typename TypeParam::RightHandSideType b = std::numeric_limits<typename TypeParam::RightHandSideType>::min();
    const typename TypeParam::LeftHandSideType  c = libcyphal::util::saturating_sub(a, b);
    if (std::numeric_limits<typename TypeParam::LeftHandSideType>::is_signed ==
        std::numeric_limits<typename TypeParam::RightHandSideType>::is_signed)
    {
        ASSERT_EQ(static_cast<typename TypeParam::LeftHandSideType>(0), c);
    }
    else
    {
        ASSERT_EQ((std::numeric_limits<typename TypeParam::LeftHandSideType>::max() >> 1U) +
                      static_cast<typename TypeParam::LeftHandSideType>(1),
                  c);
    }
}

TYPED_TEST(SaturatedMathTest, SaturatingSubMaxMin)
{
    const typename TypeParam::LeftHandSideType  a = std::numeric_limits<typename TypeParam::LeftHandSideType>::max();
    const typename TypeParam::RightHandSideType b = std::numeric_limits<typename TypeParam::RightHandSideType>::min();
    const typename TypeParam::LeftHandSideType  c = libcyphal::util::saturating_sub(a, b);
    ASSERT_EQ(std::numeric_limits<typename TypeParam::LeftHandSideType>::max(), c);
}

TYPED_TEST(SaturatedMathTest, SaturatingSubMaxMinPlusOne)
{
    const typename TypeParam::LeftHandSideType  a = std::numeric_limits<typename TypeParam::LeftHandSideType>::max();
    const typename TypeParam::RightHandSideType b =
        std::numeric_limits<typename TypeParam::RightHandSideType>::min() + 1;
    const typename TypeParam::LeftHandSideType c = libcyphal::util::saturating_sub(a, b);
    if (std::numeric_limits<typename TypeParam::RightHandSideType>::is_signed)
    {
        ASSERT_EQ(std::numeric_limits<typename TypeParam::LeftHandSideType>::max(), c);
    }
    else
    {
        ASSERT_EQ(std::numeric_limits<typename TypeParam::LeftHandSideType>::max() -
                      static_cast<typename TypeParam::LeftHandSideType>(1),
                  c);
    }
}

TYPED_TEST(SaturatedMathTest, SaturatingSubMinPlusOneMin)
{
    const typename TypeParam::LeftHandSideType a = std::numeric_limits<typename TypeParam::LeftHandSideType>::min() + 1;
    const typename TypeParam::RightHandSideType b = std::numeric_limits<typename TypeParam::RightHandSideType>::min();
    const typename TypeParam::LeftHandSideType  c = libcyphal::util::saturating_sub(a, b);
    if (std::numeric_limits<typename TypeParam::RightHandSideType>::is_signed ==
        std::numeric_limits<typename TypeParam::LeftHandSideType>::is_signed)
    {
        ASSERT_EQ(static_cast<typename TypeParam::LeftHandSideType>(1), c);
    }
    else
    {
        ASSERT_EQ((std::numeric_limits<typename TypeParam::LeftHandSideType>::max() >> 1) + 2, c);
    }
}

TYPED_TEST(SaturatedMathTest, SaturatingSubMaxFromZero)
{
    const typename TypeParam::LeftHandSideType  a = 0;
    const typename TypeParam::RightHandSideType b = std::numeric_limits<typename TypeParam::RightHandSideType>::max();

    const typename TypeParam::LeftHandSideType c = libcyphal::util::saturating_sub(a, b);
    ASSERT_LE(std::numeric_limits<typename TypeParam::LeftHandSideType>::min(), c);

    const typename TypeParam::LeftHandSideType d = libcyphal::util::saturating_sub(c, b);
    ASSERT_EQ(std::numeric_limits<typename TypeParam::LeftHandSideType>::min(), d);
}

TYPED_TEST(SaturatedMathTest, SaturatingSubZeroFromMax)
{
    const typename TypeParam::LeftHandSideType  a = std::numeric_limits<typename TypeParam::LeftHandSideType>::max();
    const typename TypeParam::RightHandSideType b = 0;

    const typename TypeParam::LeftHandSideType c = libcyphal::util::saturating_sub(a, b);
    ASSERT_LE(std::numeric_limits<typename TypeParam::LeftHandSideType>::max(), c);
}

}  // namespace