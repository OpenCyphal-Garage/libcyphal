/// @file
/// Unit tests for span.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/cetl.hpp"
#include "cetlvast/helpers.hpp"
#include "cetl/pf20/span.hpp"

namespace
{
// +----------------------------------------------------------------------+
// | DEBUG ASSERT TESTS
// +----------------------------------------------------------------------+
#if CETL_ENABLE_DEBUG_ASSERT

static void TestStaticSpanWithWrongSize()
{
    const char* hello_world = "Hello World";
    (void) cetl::pf20::span<const char, 11>(hello_world, 10);
}

TEST(DeathTestSpanAssertions, TestStaticSpanWithWrongSize)
{
    EXPECT_DEATH(TestStaticSpanWithWrongSize(), "CDE_span_001");
}

// +----------------------------------------------------------------------+

static void TestStaticSpanWithWrongDistance()
{
    const char* hello_world = "Hello World";
    (void) cetl::pf20::span<const char, 10>(hello_world, &hello_world[11]);
}

TEST(DeathTestSpanAssertions, TestStaticSpanWithWrongDistance)
{
    EXPECT_DEATH(TestStaticSpanWithWrongDistance(), "CDE_span_002");
}

// +----------------------------------------------------------------------+

static void TestDynamicSpanWithNegativeDistance()
{
    const char* hello_world = "Hello World";
    (void) cetl::pf20::span<const char>(&hello_world[11], hello_world);
}

TEST(DeathTestSpanAssertions, TestDynamicSpanWithNegativeDistance)
{
    EXPECT_DEATH(TestDynamicSpanWithNegativeDistance(), "CDE_span_012");
}

// +----------------------------------------------------------------------+

static void TestStaticSpanFromDynamicOfWrongSize()
{
    const char*            hello_world = "Hello World";
    cetl::pf20::span<const char> hello_span(hello_world, 11);
    (void) cetl::pf20::span<const char, 10>(hello_span);
}

TEST(DeathTestSpanAssertions, TestStaticSpanFromDynamicOfWrongSize)
{
    EXPECT_DEATH(TestStaticSpanFromDynamicOfWrongSize(), "CDE_span_003");
}

// +----------------------------------------------------------------------+

static void TestStaticSpanIndexPastEnd()
{
    const char*                hello_world = "Hello World";
    cetl::pf20::span<const char, 11> subject(hello_world, 11);
    (void) subject[11];
}

TEST(DeathTestSpanAssertions, TestStaticSpanIndexPastEnd)
{
    EXPECT_DEATH(TestStaticSpanIndexPastEnd(), "CDE_span_004");
}

// +----------------------------------------------------------------------+

static void TestDynamicSpanIndexPastEnd()
{
    const char*            hello_world = "Hello World";
    cetl::pf20::span<const char> subject(hello_world, 11);
    (void) subject[11];
}

TEST(DeathTestSpanAssertions, TestDynamicSpanIndexPastEnd)
{
    EXPECT_DEATH(TestDynamicSpanIndexPastEnd(), "CDE_span_013");
}

// +----------------------------------------------------------------------+

static void TestStaticIndexOfNull()
{
    const char*                hello_world = nullptr;
    cetl::pf20::span<const char, 11> subject(hello_world, 11);
    (void) subject[0];
}

TEST(DeathTestSpanAssertions, TestStaticIndexOfNull)
{
    EXPECT_DEATH(TestStaticIndexOfNull(), "CDE_span_005");
}

// +----------------------------------------------------------------------+

static void TestDynamicIndexOfNull()
{
    const char*            hello_world = nullptr;
    cetl::pf20::span<const char> subject(hello_world, 1);
    (void) subject[0];
}

TEST(DeathTestSpanAssertions, TestDynamicIndexOfNull)
{
    EXPECT_DEATH(TestDynamicIndexOfNull(), "CDE_span_014");
}

// +----------------------------------------------------------------------+

static void TestStaticFrontOfZeroSize()
{
    const char*               hello_world = "Hello world";
    cetl::pf20::span<const char, 0> subject(hello_world, 0);
    (void) subject.front();
}

TEST(DeathTestSpanAssertions, TestStaticFrontOfZeroSize)
{
    EXPECT_DEATH(TestStaticFrontOfZeroSize(), "CDE_span_006");
}

// +----------------------------------------------------------------------+

static void TestDynamicFrontOfZeroSize()
{
    const char*            hello_world = "Hello world";
    cetl::pf20::span<const char> subject(hello_world, 0);
    (void) subject.front();
}

TEST(DeathTestSpanAssertions, TestDynamicFrontOfZeroSize)
{
    EXPECT_DEATH(TestDynamicFrontOfZeroSize(), "CDE_span_015");
}

// +----------------------------------------------------------------------+

static void TestStaticBackOfZeroSize()
{
    const char*               hello_world = "Hello world";
    cetl::pf20::span<const char, 0> subject(hello_world, 0);
    (void) subject.back();
}

TEST(DeathTestSpanAssertions, TestStaticBackOfZeroSize)
{
    EXPECT_DEATH(TestStaticBackOfZeroSize(), "CDE_span_007");
}

// +----------------------------------------------------------------------+

static void TestDynamicBackOfZeroSize()
{
    const char*            hello_world = "Hello world";
    cetl::pf20::span<const char> subject(hello_world, 0);
    (void) subject.back();
}

TEST(DeathTestSpanAssertions, TestDynamicBackOfZeroSize)
{
    EXPECT_DEATH(TestDynamicBackOfZeroSize(), "CDE_span_016");
}

// +----------------------------------------------------------------------+

static void TestDynamicSubviewToStaticFirstTooLarge()
{
    const char*            hello_world = "Hello world";
    cetl::pf20::span<const char> subject(hello_world, 10);
    (void) subject.first<11>();
}

TEST(DeathTestSpanAssertions, TestDynamicSubviewToStaticFirstTooLarge)
{
    EXPECT_DEATH(TestDynamicSubviewToStaticFirstTooLarge(), "CDE_span_017");
}

// +----------------------------------------------------------------------+

static void TestStaticSubviewFirstTooLarge()
{
    const char*                hello_world = "Hello world";
    cetl::pf20::span<const char, 10> subject(hello_world, 10);
    (void) subject.first(11);
}

TEST(DeathTestSpanAssertions, TestStaticSubviewFirstTooLarge)
{
    EXPECT_DEATH(TestStaticSubviewFirstTooLarge(), "CDE_span_008");
}

// +----------------------------------------------------------------------+

static void TestDynamicSubviewFirstTooLarge()
{
    const char*            hello_world = "Hello world";
    cetl::pf20::span<const char> subject(hello_world, 10);
    (void) subject.first(11);
}

TEST(DeathTestSpanAssertions, TestDynamicSubviewFirstTooLarge)
{
    EXPECT_DEATH(TestDynamicSubviewFirstTooLarge(), "CDE_span_018");
}

// +----------------------------------------------------------------------+

static void TestDynamicSubviewToStaticLastTooLarge()
{
    const char*            hello_world = "Hello world";
    cetl::pf20::span<const char> subject(hello_world, 10);
    (void) subject.last<11>();
}

TEST(DeathTestSpanAssertions, TestDynamicSubviewToStaticLastTooLarge)
{
    EXPECT_DEATH(TestDynamicSubviewToStaticLastTooLarge(), "CDE_span_019");
}

// +----------------------------------------------------------------------+

static void TestStaticSubviewLastTooLarge()
{
    const char*                hello_world = "Hello world";
    cetl::pf20::span<const char, 10> subject(hello_world, 10);
    (void) subject.last(11);
}

TEST(DeathTestSpanAssertions, TestStaticSubviewLastTooLarge)
{
    EXPECT_DEATH(TestStaticSubviewLastTooLarge(), "CDE_span_009");
}

// +----------------------------------------------------------------------+

static void TestDynamicSubviewLastTooLarge()
{
    const char*            hello_world = "Hello world";
    cetl::pf20::span<const char> subject(hello_world, 10);
    (void) subject.last(11);
}

TEST(DeathTestSpanAssertions, TestDynamicSubviewLastTooLarge)
{
    EXPECT_DEATH(TestDynamicSubviewLastTooLarge(), "CDE_span_020");
}

// +----------------------------------------------------------------------+

static void TestDynamicSubspanToStaticOffsetTooLarge()
{
    const char*            hello_world = "Hello world";
    cetl::pf20::span<const char> subject(hello_world, 10);
    (void) subject.subspan<11>();
}

TEST(DeathTestSpanAssertions, TestDynamicSubspanToStaticOffsetTooLarge)
{
    EXPECT_DEATH(TestDynamicSubspanToStaticOffsetTooLarge(), "CDE_span_023");
}

// +----------------------------------------------------------------------+
static void TestDynamicSubspanToStaticOffsetAndSizeTooLarge()
{
    const char*            hello_world = "Hello world";
    cetl::pf20::span<const char> subject(hello_world, 10);
    (void) subject.subspan<1, 11>();
}

static void TestDynamicSubspanToStaticOffsetAndSizeTooLargeHappy()
{
    const char*            hello_world = "Hello world";
    cetl::pf20::span<const char> subject(hello_world, 10);
    (void) subject.subspan<1>();
}

TEST(DeathTestSpanAssertions, TestDynamicSubspanToStaticOffsetAndSizeTooLarge)
{
    // happy case
    TestDynamicSubspanToStaticOffsetAndSizeTooLargeHappy();
    // now we all die!
    EXPECT_DEATH(TestDynamicSubspanToStaticOffsetAndSizeTooLarge(), "CDE_span_024");
}

// +----------------------------------------------------------------------+

static void TestStaticSubspanOffsetTooLarge()
{
    const char*                hello_world = "Hello world";
    cetl::pf20::span<const char, 10> subject(hello_world, 10);
    (void) subject.subspan(11, cetl::pf20::dynamic_extent);
}

TEST(DeathTestSpanAssertions, TestStaticSubspanOffsetTooLarge)
{
    EXPECT_DEATH(TestStaticSubspanOffsetTooLarge(), "CDE_span_010");
}

// +----------------------------------------------------------------------+

static void TestDynamicSubspanOffsetTooLarge()
{
    const char*            hello_world = "Hello world";
    cetl::pf20::span<const char> subject(hello_world, 10);
    (void) subject.subspan(11, cetl::pf20::dynamic_extent);
}

TEST(DeathTestSpanAssertions, TestDynamicSubspanOffsetTooLarge)
{
    EXPECT_DEATH(TestDynamicSubspanOffsetTooLarge(), "CDE_span_021");
}

// +----------------------------------------------------------------------+

static void TestDynamicSubspanOffsetAndSizeTooLarge(std::size_t offset, std::size_t count)
{
    const char*            hello_world = "Hello world";
    cetl::pf20::span<const char> subject(hello_world, 10);
    (void) subject.subspan(offset, count);
}

TEST(DeathTestSpanAssertions, TestDynamicSubspanOffsetAndSizeTooLarge)
{
    // happy case
    TestDynamicSubspanOffsetAndSizeTooLarge(0, cetl::pf20::dynamic_extent);
    // now we all die!
    EXPECT_DEATH(
        {
            const std::size_t offset = 1;
            const std::size_t count  = 10;
            TestDynamicSubspanOffsetAndSizeTooLarge(offset, count);
        },
        "CDE_span_022");
}

// +----------------------------------------------------------------------+

static void TestStaticSubspanCountIsWrong()
{
    const char*                hello_world = "Hello world";
    cetl::pf20::span<const char, 10> subject(hello_world, 10);
    (void) subject.subspan(10, 1);
}

TEST(DeathTestSpanAssertions, TestStaticSubspanCountIsWrong)
{
    EXPECT_DEATH(TestStaticSubspanCountIsWrong(), "CDE_span_011");
}

#endif  // CETL_ENABLE_DEBUG_ASSERT

}  //  namespace
