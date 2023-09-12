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
#include <vector>
#include <initializer_list>
#include <type_traits>
#include <algorithm>
#include <utility>

#if (__cplusplus >= CETL_CPP_STANDARD_20)
#    include <version>

static_assert(__cpp_lib_span, "__cpp_lib_span was not defined for this compiler when using 2020.02?");
#    include <span>
static_assert(std::numeric_limits<std::size_t>::max() == std::dynamic_extent,
              "These tests assume std::dynamic_extent is the max for size_t.");
#endif

#include "cetl/pf20/span.hpp"
static_assert(std::numeric_limits<std::size_t>::max() == cetl::pf20::dynamic_extent,
              "These tests assume cetl::pf20::dynamic_extent is the max for size_t.");

namespace
{
// +----------------------------------------------------------------------+
// | Test helpers
// +----------------------------------------------------------------------+
///
/// Test helper for constructing data arrays to use for span tests.
template <typename SpanType, std::size_t Extent = SpanType::extent>
class SpanData;

template <typename SpanType, std::size_t Extent>
class SpanData
{
public:
    static constexpr std::size_t default_dynamic_data_len = 12;
    static constexpr std::size_t data_len = (Extent == cetl::pf20::dynamic_extent) ? default_dynamic_data_len : Extent;
    using span_type                       = SpanType;
    using element_type                    = typename SpanType::element_type;
    using value_type                      = typename SpanType::value_type;

    SpanData()
        : data_{}
    {
        for (std::size_t i = 0; i < data_len; ++i)
        {
            data_[i] = static_cast<value_type>(i) + 1;
        }
    }

    explicit SpanData(typename SpanType::value_type&& fill_value)
        : data_{}
    {
        std::fill(data_, &data_[data_len], fill_value);
    }

    SpanData(std::initializer_list<typename SpanType::value_type> l)
        : data_{}
    {
        memcpy(data_, std::begin(l), sizeof(typename SpanType::value_type) * data_len);
    }

    element_type (&data())[data_len]
    {
        return data_;
    }

    value_type (&mutable_data())[data_len]
    {
        return data_;
    }

    template <typename DeducedT, typename std::enable_if<!std::is_const<DeducedT>::value, bool>::type = true>
    static constexpr std::array<DeducedT, data_len> asArray(DeducedT (&data)[data_len])
    {
        return makeArray(data, std::make_index_sequence<data_len>{});
    }

    template <typename DeducedT, typename std::enable_if<std::is_const<DeducedT>::value, bool>::type = true>
    static constexpr const std::array<DeducedT, data_len> asArray(DeducedT (&data)[data_len])
    {
        return makeArray(data, std::make_index_sequence<data_len>{});
    }

private:
    template <typename DeducedT, std::size_t data_len, std::size_t... I>
    static constexpr std::array<DeducedT, data_len> makeArray(DeducedT (&data)[data_len], std::index_sequence<I...>)
    {
        return {{data[I]...}};
    }

    value_type data_[data_len];
};

// required till C++ 17. Redundant but allowed after that.
template <typename SpanType, std::size_t Extent>
const std::size_t SpanData<SpanType, Extent>::data_len;

template <typename SpanType>
class SpanData<SpanType, 0>
{
public:
    static constexpr std::size_t data_len = 0;
    using span_type                       = SpanType;
    using element_type                    = typename SpanType::element_type;
    using value_type                      = typename SpanType::value_type;

    SpanData()
        : SpanData({})
    {
    }

    explicit SpanData(value_type&& fill_value)
        : data_(nullptr)
    {
        (void) fill_value;
    }

    SpanData(std::initializer_list<value_type> l)
        : data_(nullptr)
    {
        (void) l;
    }

    element_type* data()
    {
        return data_;
    }

    value_type* mutable_data()
    {
        return data_;
    }

private:
    value_type* data_;
};

// required till C++ 17. Redundant but allowed after that.
template <typename SpanType>
const std::size_t SpanData<SpanType, 0>::data_len;

// +----------------------------------------------------------------------+
// | Test Suite(s)
// +----------------------------------------------------------------------+
/**
 * Test suite for testing cetl::pf20::span against C++20 span where the latter
 * is available. Otherwise this will only test cetl::pf20::span in isolation.
 */
template <typename T>
class TestSpan : public ::testing::Test
{};

using SpanImplementations = ::testing::Types<cetl::pf20::span<int, 0>,
                                             cetl::pf20::span<int, 16>,
                                             cetl::pf20::span<const int, 3>,
                                             cetl::pf20::span<const int>,
                                             cetl::pf20::span<int>
#ifdef __cpp_lib_span
                                             ,
                                             std::span<int, 0>,
                                             std::span<int, 16>,
                                             std::span<const int, 3>,
                                             std::span<const int>,
                                             std::span<int>
#endif
                                             >;

TYPED_TEST_SUITE(TestSpan, SpanImplementations, );

// +----------------------------------------------------------------------+
// Required properties where extent is dynamic or 0
template <typename T,
          std::size_t DeducedExtent           = T::extent,
          typename std::enable_if<(DeducedExtent == 0 || DeducedExtent == std::numeric_limits<std::size_t>::max()),
                                  bool>::type = true>
static void assertSpanInterface()
{
    T subject;
    ASSERT_TRUE(subject.empty());
    static_assert(noexcept(T()), "Default ctor must be noexcept.");
    static_assert(noexcept(T(T())), "Copy ctor must be noexcept.");
    static_assert(noexcept(T().empty()), "empty() method must be noexcept.");
    static_assert(noexcept(T().begin()), "begin() method must be noexcept.");
    static_assert(noexcept(T().rbegin()), "rbegin() method must be noexcept.");
    static_assert(noexcept(T().end()), "end() method must be noexcept.");
    static_assert(noexcept(T().rend()), "rend() method must be noexcept.");
    static_assert(noexcept(T().operator=(std::declval<T>())), "assignment operator must be noexcept.");
    static_assert(noexcept(T().size()), "size() method must be noexcept.");
    static_assert(noexcept(T().size_bytes()), "size_bytes() method must be noexcept.");
    static_assert(noexcept(T().data()), "data() method must be noexcept.");

    ASSERT_EQ(subject.begin(), subject.end());
}

// Required properties where extent is static
template <typename T,
          std::size_t DeducedExtent           = T::extent,
          typename std::enable_if<(DeducedExtent != 0 && DeducedExtent != std::numeric_limits<std::size_t>::max()),
                                  bool>::type = true>
void assertSpanInterface()
{
    static_assert(noexcept(std::declval<T>()), "Default ctor must be noexcept.");
    static_assert(noexcept(std::declval<T>().empty()), "empty method must be noexcept.");
    static_assert(noexcept(std::declval<T>().begin()), "begin() method must be noexcept.");
    static_assert(noexcept(std::declval<T>().rbegin()), "rbegin() method must be noexcept.");
    static_assert(noexcept(std::declval<T>().end()), "end() method must be noexcept.");
    static_assert(noexcept(std::declval<T>().rend()), "rend() method must be noexcept.");
    static_assert(noexcept(std::declval<T>().operator=(std::declval<T>())), "assignment operator must be noexcept.");
    static_assert(noexcept(std::declval<T>().size()), "size() method must be noexcept.");
    static_assert(noexcept(std::declval<T>().size_bytes()), "size_bytes() method must be noexcept.");
    static_assert(noexcept(std::declval<T>().data()), "data() method must be noexcept.");
}

TYPED_TEST(TestSpan, TestDefaultCtor)
{
    assertSpanInterface<TypeParam>();
}

// +----------------------------------------------------------------------+
TYPED_TEST(TestSpan, TestFirstAndCountCtor)
{
    if (TypeParam::extent > 0)
    {
        SpanData<TypeParam> testData(0xAA);
        TypeParam           subject(testData.data(), SpanData<TypeParam>::data_len);
        ASSERT_FALSE(subject.empty());
        ASSERT_NE(subject.begin(), subject.end());
        for (typename TypeParam::iterator i = subject.begin(), e = subject.end(); i != e; ++i)
        {
            ASSERT_EQ(0xAA, *i);
        }
    }
}

// +----------------------------------------------------------------------+
TYPED_TEST(TestSpan, TestFirstAndLastCtor)
{
    if (TypeParam::extent > 0)
    {
        SpanData<TypeParam>         testData(0xAA);
        typename TypeParam::pointer first = testData.data();
        typename TypeParam::pointer end   = first + SpanData<TypeParam>::data_len;
        ASSERT_GE(end, first) << "First " << end << " was after last " << end << '\n';
        TypeParam subject(first, end);
        ASSERT_FALSE(subject.empty());
        ASSERT_NE(subject.begin(), subject.end());
        for (typename TypeParam::iterator i = subject.begin(), e = subject.end(); i != e; ++i)
        {
            ASSERT_EQ(0xAA, *i);
        }
    }
}

// +----------------------------------------------------------------------+
template <typename TypeParam, std::size_t Extent = TypeParam::extent>
struct TestArrayCtorIfNotZero;

template <typename TypeParam, std::size_t Extent>
struct TestArrayCtorIfNotZero
{
    void operator()()
    {
        SpanData<TypeParam> testData(0xAA);
        TypeParam           subject(testData.data());
        ASSERT_FALSE(subject.empty());
        ASSERT_EQ(SpanData<TypeParam>::data_len, subject.size());
        ASSERT_NE(subject.begin(), subject.end());
        for (typename TypeParam::iterator i = subject.begin(), e = subject.end(); i != e; ++i)
        {
            ASSERT_EQ(0xAA, *i);
        }
    }
};

template <typename TypeParam>
struct TestArrayCtorIfNotZero<TypeParam, 0>
{
    // nothing to test.
    void operator()() {}
};

TYPED_TEST(TestSpan, TestArrayCtor)
{
    TestArrayCtorIfNotZero<TypeParam> test;
    test();
}

// +----------------------------------------------------------------------+
template <typename TypeParam, std::size_t Extent = TypeParam::extent>
struct TestStlArrayCtorIfNotZero;

template <typename TypeParam, std::size_t Extent>
struct TestStlArrayCtorIfNotZero
{
    template <
        typename DeducedTypeParam,
        typename std::enable_if<!std::is_const<typename DeducedTypeParam::element_type>::value, bool>::type = true>
    void operator()(SpanData<DeducedTypeParam>&& testData)
    {
        auto stlArray =
            SpanData<DeducedTypeParam>::template asArray<typename DeducedTypeParam::element_type>(testData.data());
        common_assertions(DeducedTypeParam(stlArray));
    }
    template <typename DeducedTypeParam,
              typename std::enable_if<std::is_const<typename DeducedTypeParam::element_type>::value, bool>::type = true>
    void operator()(SpanData<DeducedTypeParam>&& testData)
    {
        const auto stlArray =
            SpanData<DeducedTypeParam>::template asArray<typename DeducedTypeParam::element_type>(testData.data());
        common_assertions(DeducedTypeParam(stlArray));
    }

private:
    template <typename DeducedTypeParam>
    void common_assertions(DeducedTypeParam&& subject)
    {
        ASSERT_FALSE(subject.empty());
        ASSERT_EQ(SpanData<DeducedTypeParam>::data_len, subject.size());
        ASSERT_NE(subject.begin(), subject.end());
        for (typename DeducedTypeParam::iterator i = subject.begin(), e = subject.end(); i != e; ++i)
        {
            ASSERT_EQ(0xAA, *i);
        }
    }
};

template <typename TypeParam>
struct TestStlArrayCtorIfNotZero<TypeParam, 0>
{
    // nothing to test.
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&&)
    {
    }
};

TYPED_TEST(TestSpan, TestStlArrayCtor)
{
    TestStlArrayCtorIfNotZero<TypeParam> test;
    test(SpanData<TypeParam>(0xAA));
}

// +----------------------------------------------------------------------+
TYPED_TEST(TestSpan, TestFront)
{
    if (TypeParam::extent > 0)
    {
        SpanData<TypeParam> testData{0xAA, 0};
        TypeParam           subject(testData.data(), SpanData<TypeParam>::data_len);
        ASSERT_EQ(0xAA, subject.front());
    }
}

// +----------------------------------------------------------------------+
TYPED_TEST(TestSpan, TestBack)
{
    if (TypeParam::extent > 0)
    {
        SpanData<TypeParam> testData(0);
        testData.mutable_data()[SpanData<TypeParam>::data_len - 1] = 0xAA;
        TypeParam subject(testData.data(), SpanData<TypeParam>::data_len);
        ASSERT_EQ(0xAA, subject.back());
    }
}

// +----------------------------------------------------------------------+
TYPED_TEST(TestSpan, TestBeginAndEnd)
{
    SpanData<TypeParam> testData(0xAA);
    TypeParam           subject(testData.data(), SpanData<TypeParam>::data_len);
    std::size_t         item_count = 0;
    for (auto i : subject)
    {
        ASSERT_EQ(0xAA, i);
        item_count += 1;
    }
    ASSERT_EQ(item_count, subject.size());
}

// +----------------------------------------------------------------------+
TYPED_TEST(TestSpan, TestRBeginAndREnd)
{
    SpanData<TypeParam> testData(0xAA);
    TypeParam           subject(testData.data(), SpanData<TypeParam>::data_len);
    std::size_t         item_count = 0;
    for (typename TypeParam::reverse_iterator i = subject.rbegin(), e = subject.rend(); i != e; ++i)
    {
        ASSERT_EQ(0xAA, *i);
        item_count += 1;
    }
    ASSERT_EQ(item_count, subject.size());
}

// +----------------------------------------------------------------------+
TYPED_TEST(TestSpan, TestAssignment)
{
    SpanData<TypeParam> testDataFixture{1, 2};
    SpanData<TypeParam> testData{3, 4};
    TypeParam           fixture(testDataFixture.data(), SpanData<TypeParam>::data_len);
    TypeParam           subject(testData.data(), SpanData<TypeParam>::data_len);
    subject = fixture;
    if (SpanData<TypeParam>::data_len >= 1)
    {
        ASSERT_EQ(1, subject.front());
    }
    if (SpanData<TypeParam>::data_len >= 2)
    {
        ASSERT_EQ(2, *(subject.begin() + 1));
    }
}

// +----------------------------------------------------------------------+
TYPED_TEST(TestSpan, TestSize)
{
    SpanData<TypeParam> testData(0xAA);
    TypeParam           subject(testData.data(), SpanData<TypeParam>::data_len);
    ASSERT_EQ(SpanData<TypeParam>::data_len, subject.size());
    if (TypeParam::extent != cetl::pf20::dynamic_extent)
    {
        ASSERT_EQ(TypeParam::extent, subject.size());
    }
}

// +----------------------------------------------------------------------+
TYPED_TEST(TestSpan, TestSizeBytes)
{
    SpanData<TypeParam> testData(0xAA);
    TypeParam           subject(testData.data(), SpanData<TypeParam>::data_len);
    ASSERT_EQ(sizeof(typename TypeParam::element_type) * SpanData<TypeParam>::data_len, subject.size_bytes());
    if (TypeParam::extent != cetl::pf20::dynamic_extent)
    {
        ASSERT_EQ(sizeof(typename TypeParam::element_type) * TypeParam::extent, subject.size_bytes());
    }
}

// +----------------------------------------------------------------------+
TYPED_TEST(TestSpan, TestBrackets)
{
    SpanData<TypeParam> testData(0xAA);
    TypeParam           subject(testData.data(), SpanData<TypeParam>::data_len);

    for (std::size_t i = 0; i < subject.size(); ++i)
    {
        ASSERT_EQ(0xAA, subject[i]);
    }
}

// +----------------------------------------------------------------------+
TYPED_TEST(TestSpan, TestData)
{
    SpanData<TypeParam> testData(0xAA);
    TypeParam           subject(testData.data(), SpanData<TypeParam>::data_len);
    if (subject.size() == 0)
    {
        ASSERT_EQ(nullptr, subject.data());
    }
    else
    {
        ASSERT_NE(nullptr, subject.data());
    }

    for (std::size_t i = 0; i < subject.size(); ++i)
    {
        ASSERT_EQ(0xAA, subject.data()[i]);
    }
}

// +----------------------------------------------------------------------+
template <typename TypeParam, typename = std::integral_constant<bool, true>>
struct TestSubviewFirstByExtent;

template <typename TypeParam>
struct TestSubviewFirstByExtent<
    TypeParam,
    std::integral_constant<bool, ((TypeParam::extent >= 2) && (TypeParam::extent < cetl::pf20::dynamic_extent))>>
{
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&& testData)
    {
        constexpr std::size_t subextent = TypeParam::extent - 1;
        TypeParam             subject(testData.data(), TypeParam::extent);
        auto                  subview = subject.template first<subextent>();
        ASSERT_EQ(decltype(subview)::extent, subextent);
        ASSERT_EQ(subview.size(), subextent);
        ASSERT_NE(subject.size(), subview.size());
        for (std::size_t i = 0; i < subview.size(); ++i)
        {
            ASSERT_EQ(i, subview[i] - 1);
        }
    }
};

template <typename TypeParam>
struct TestSubviewFirstByExtent<TypeParam, std::integral_constant<bool, (TypeParam::extent == cetl::pf20::dynamic_extent)>>
{
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&& testData)
    {
        constexpr std::size_t subextent = SpanData<DeducedTypeParam>::data_len - 1;
        TypeParam             subject(testData.data(), TypeParam::extent);
        auto                  subview = subject.template first<subextent>();
        ASSERT_EQ(decltype(subview)::extent, subextent);
        ASSERT_EQ(subview.size(), subextent);
        ASSERT_NE(subject.size(), subview.size());
        for (std::size_t i = 0; i < subview.size(); ++i)
        {
            ASSERT_EQ(i, subview[i] - 1);
        }
    }
};

template <typename TypeParam>
struct TestSubviewFirstByExtent<TypeParam, std::integral_constant<bool, (TypeParam::extent < 2)>>
{
    // nothing to test.
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&&)
    {
    }
};

TYPED_TEST(TestSpan, TestSubviewFirst)
{
    TestSubviewFirstByExtent<TypeParam> test;
    test(SpanData<TypeParam>());
}

// +----------------------------------------------------------------------+
template <typename TypeParam, typename = std::integral_constant<bool, true>>
struct TestSubviewLastByExtent;

template <typename TypeParam>
    struct TestSubviewLastByExtent < TypeParam,
    std::integral_constant<bool, TypeParam::extent >= 2 && TypeParam::extent<cetl::pf20::dynamic_extent>>
{
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&& testData)
    {
        constexpr std::size_t subextent = TypeParam::extent - 1;
        TypeParam             subject(testData.data(), TypeParam::extent);
        auto                  subview = subject.template last<subextent>();
        ASSERT_EQ(decltype(subview)::extent, subextent);
        ASSERT_EQ(subview.size(), subextent);
        ASSERT_NE(subject.size(), subview.size());
        for (std::size_t i = 0; i < subextent; ++i)
        {
            ASSERT_EQ(i + (TypeParam::extent - subextent), subview[i] - 1);
        }
    }
};

template <typename TypeParam>
struct TestSubviewLastByExtent<TypeParam, std::integral_constant<bool, TypeParam::extent == cetl::pf20::dynamic_extent>>
{
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&& testData)
    {
        constexpr std::size_t subextent = SpanData<DeducedTypeParam>::data_len - 1;
        TypeParam             subject(testData.data());
        auto                  subview = subject.template last<subextent>();
        ASSERT_EQ(decltype(subview)::extent, subextent);
        ASSERT_EQ(subview.size(), subextent);
        ASSERT_NE(subject.size(), subview.size());
        for (std::size_t i = 0; i < subextent; ++i)
        {
            ASSERT_EQ(i + (SpanData<DeducedTypeParam>::data_len - subextent), subview[i] - 1);
        }
    }
};

template <typename TypeParam>
    struct TestSubviewLastByExtent < TypeParam,
    std::integral_constant<bool, TypeParam::extent<2>>
{
    // nothing to test.
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&&)
    {
    }
};

TYPED_TEST(TestSpan, TestSubviewLast)
{
    TestSubviewLastByExtent<TypeParam> test;
    test(SpanData<TypeParam>());
}

// +----------------------------------------------------------------------+
template <typename TypeParam, typename = std::integral_constant<bool, true>>
struct TestSubviewSubspanByExtent;

template <typename TypeParam>
    struct TestSubviewSubspanByExtent < TypeParam,
    std::integral_constant<bool, TypeParam::extent >= 3 && TypeParam::extent<cetl::pf20::dynamic_extent>>
{
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&& testData)
    {
        TypeParam improper_subject(testData.data(), TypeParam::extent);
        auto      improper_subview = improper_subject.template subspan<0, TypeParam::extent>();
        ASSERT_EQ(decltype(improper_subview)::extent, TypeParam::extent);
        ASSERT_EQ(improper_subview.size(), TypeParam::extent);
        ASSERT_EQ(improper_subview.size(), improper_subject.size());
        for (std::size_t i = 0; i < improper_subview.size(); ++i)
        {
            ASSERT_EQ(i, improper_subview[i] - 1);
        }

        constexpr std::size_t offset = 1;
        constexpr std::size_t count  = TypeParam::extent - 2;

        TypeParam proper_subject(testData.data(), TypeParam::extent);
        auto      proper_subview = proper_subject.template subspan<offset, count>();
        ASSERT_EQ(decltype(proper_subview)::extent, count);
        ASSERT_EQ(proper_subview.size(), count);
        ASSERT_NE(proper_subview.size(), proper_subject.size());
        for (std::size_t i = 0; i < proper_subview.size(); ++i)
        {
            ASSERT_EQ(i + offset, proper_subview[i] - 1);
        }

        auto proper_subview_dcount = proper_subject.template subspan<offset>();
        ASSERT_EQ(decltype(proper_subview_dcount)::extent, TypeParam::extent - offset);
        ASSERT_EQ(proper_subview_dcount.size(), TypeParam::extent - offset);
        ASSERT_NE(proper_subview_dcount.size(), proper_subject.size());
        for (std::size_t i = 0; i < proper_subview_dcount.size(); ++i)
        {
            ASSERT_EQ(i + offset, proper_subview_dcount[i] - 1);
        }
    }
};

template <typename TypeParam>
struct TestSubviewSubspanByExtent<TypeParam, std::integral_constant<bool, TypeParam::extent == cetl::pf20::dynamic_extent>>
{
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&& testData)
    {
        constexpr std::size_t data_extent = SpanData<DeducedTypeParam>::data_len;
        TypeParam             improper_subject(testData.data(), data_extent);
        auto                  improper_subview = improper_subject.template subspan<0, data_extent>();
        ASSERT_EQ(decltype(improper_subview)::extent, data_extent);
        ASSERT_EQ(improper_subview.size(), data_extent);
        ASSERT_EQ(improper_subview.size(), improper_subject.size());
        for (std::size_t i = 0; i < improper_subview.size(); ++i)
        {
            ASSERT_EQ(i, improper_subview[i] - 1);
        }

        constexpr std::size_t offset = 1;
        constexpr std::size_t count  = data_extent - 2;

        TypeParam proper_subject(testData.data(), data_extent);
        auto      proper_subview = proper_subject.template subspan<offset, count>();
        ASSERT_EQ(decltype(proper_subview)::extent, count);
        ASSERT_EQ(proper_subview.size(), count);
        ASSERT_NE(proper_subview.size(), proper_subject.size());
        for (std::size_t i = 0; i < proper_subview.size(); ++i)
        {
            ASSERT_EQ(i + offset, proper_subview[i] - 1);
        }

        auto proper_subview_dcount = proper_subject.template subspan<offset>();
        ASSERT_EQ(decltype(proper_subview_dcount)::extent, cetl::pf20::dynamic_extent);
        ASSERT_EQ(proper_subview_dcount.size(), data_extent - offset);
        ASSERT_NE(proper_subview_dcount.size(), proper_subject.size());
        for (std::size_t i = 0; i < proper_subview_dcount.size(); ++i)
        {
            ASSERT_EQ(i + offset, proper_subview_dcount[i] - 1);
        }
    }
};

template <typename TypeParam>
    struct TestSubviewSubspanByExtent < TypeParam,
    std::integral_constant<bool, TypeParam::extent<2>>
{
    // nothing to test.
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&&)
    {
    }
};

TYPED_TEST(TestSpan, TestSubviewSubspan)
{
    TestSubviewSubspanByExtent<TypeParam> test;
    test(SpanData<TypeParam>());
}

// +----------------------------------------------------------------------+
template <typename TypeParam, typename = std::integral_constant<bool, true>>
struct TestSubviewFirstDynamicByExtent;

template <typename TypeParam>
    struct TestSubviewFirstDynamicByExtent < TypeParam,
    std::integral_constant<bool, TypeParam::extent >= 2 && TypeParam::extent<cetl::pf20::dynamic_extent>>
{
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&& testData)
    {
        const std::size_t subextent = TypeParam::extent - 1;

        // proper sub-set
        TypeParam proper_subject(testData.data(), TypeParam::extent);
        auto      proper_subview = proper_subject.first(subextent);
        ASSERT_EQ(decltype(proper_subview)::extent, cetl::pf20::dynamic_extent);
        ASSERT_EQ(proper_subview.size(), subextent);
        ASSERT_NE(proper_subject.size(), proper_subview.size());
        for (std::size_t i = 0; i < proper_subview.size(); ++i)
        {
            ASSERT_EQ(i, proper_subview[i] - 1);
        }

        // improper sub-set
        TypeParam improper_subject(testData.data(), TypeParam::extent);
        auto      improper_subview = improper_subject.first(TypeParam::extent);
        ASSERT_EQ(decltype(improper_subview)::extent, cetl::pf20::dynamic_extent);
        ASSERT_EQ(improper_subview.size(), TypeParam::extent);
        ASSERT_EQ(improper_subject.size(), improper_subview.size());
        for (std::size_t i = 0; i < improper_subview.size(); ++i)
        {
            ASSERT_EQ(i, improper_subview[i] - 1);
        }
    }
};

template <typename TypeParam>
struct TestSubviewFirstDynamicByExtent<TypeParam,
                                       std::integral_constant<bool, TypeParam::extent == cetl::pf20::dynamic_extent>>
{
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&& testData)
    {
        TypeParam         subject(testData.data(), SpanData<DeducedTypeParam>::data_len);
        const std::size_t subextent = subject.size() - 1;
        ASSERT_NE(cetl::pf20::dynamic_extent - 1, subextent);
        auto subview = subject.first(subextent);
        ASSERT_EQ(decltype(subview)::extent, cetl::pf20::dynamic_extent);
        ASSERT_EQ(subview.size(), subextent);
        ASSERT_NE(subject.size(), subview.size());
        for (std::size_t i = 0; i < subview.size(); ++i)
        {
            ASSERT_EQ(i, subview[i] - 1);
        }
    }
};

template <typename TypeParam>
    struct TestSubviewFirstDynamicByExtent < TypeParam,
    std::integral_constant<bool, TypeParam::extent<2>>
{
    // nothing to test.
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&&)
    {
    }
};

TYPED_TEST(TestSpan, TestSubviewFirstDynamic)
{
    TestSubviewFirstDynamicByExtent<TypeParam> test;
    test(SpanData<TypeParam>());
}

// +----------------------------------------------------------------------+
template <typename TypeParam, typename = std::integral_constant<bool, true>>
struct TestSubviewLastDynamicByExtent;

template <typename TypeParam>
    struct TestSubviewLastDynamicByExtent < TypeParam,
    std::integral_constant<bool, TypeParam::extent >= 2 && TypeParam::extent<cetl::pf20::dynamic_extent>>
{
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&& testData)
    {
        constexpr std::size_t subextent = TypeParam::extent - 1;

        // proper subset
        TypeParam proper_subject(testData.data(), TypeParam::extent);
        auto      proper_subview = proper_subject.last(subextent);
        ASSERT_EQ(decltype(proper_subview)::extent, cetl::pf20::dynamic_extent);
        ASSERT_EQ(proper_subview.size(), subextent);
        ASSERT_NE(proper_subject.size(), proper_subview.size());
        for (std::size_t i = 0; i < proper_subview.size(); ++i)
        {
            ASSERT_EQ(i + (TypeParam::extent - subextent), proper_subview[i] - 1);
        }

        // improper subset
        TypeParam improper_subject(testData.data(), TypeParam::extent);
        auto      improper_subview = improper_subject.last(TypeParam::extent);
        ASSERT_EQ(decltype(improper_subview)::extent, cetl::pf20::dynamic_extent);
        ASSERT_EQ(improper_subview.size(), TypeParam::extent);
        ASSERT_EQ(improper_subject.size(), improper_subview.size());
        for (std::size_t i = 0; i < improper_subview.size(); ++i)
        {
            ASSERT_EQ(i, improper_subview[i] - 1);
        }
    }
};

template <typename TypeParam>
struct TestSubviewLastDynamicByExtent<TypeParam,
                                      std::integral_constant<bool, TypeParam::extent == cetl::pf20::dynamic_extent>>
{
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&& testData)
    {
        const std::size_t subextent = SpanData<DeducedTypeParam>::data_len - 1;
        TypeParam         subject(testData.data());
        auto              subview = subject.last(subextent);
        ASSERT_EQ(decltype(subview)::extent, cetl::pf20::dynamic_extent);
        ASSERT_EQ(subview.size(), subextent);
        ASSERT_NE(subject.size(), subview.size());
        for (std::size_t i = 0; i < subextent; ++i)
        {
            ASSERT_EQ(i + (SpanData<DeducedTypeParam>::data_len - subextent), subview[i] - 1);
        }
    }
};

template <typename TypeParam>
    struct TestSubviewLastDynamicByExtent < TypeParam,
    std::integral_constant<bool, TypeParam::extent<2>>
{
    // nothing to test.
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&&)
    {
    }
};

TYPED_TEST(TestSpan, TestSubviewLastDynamic)
{
    TestSubviewLastDynamicByExtent<TypeParam> test;
    test(SpanData<TypeParam>());
}

// +----------------------------------------------------------------------+

template <typename TypeParam, typename = std::integral_constant<bool, true>>
struct TestSubviewSubspanDynamicByExtent;

template <typename TypeParam>
    struct TestSubviewSubspanDynamicByExtent < TypeParam,
    std::integral_constant<bool, TypeParam::extent >= 3 && TypeParam::extent<cetl::pf20::dynamic_extent>>
{
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&& testData)
    {
        TypeParam improper_subject(testData.data(), TypeParam::extent);
        auto      improper_subview = improper_subject.subspan(0, TypeParam::extent);
        ASSERT_EQ(decltype(improper_subview)::extent, cetl::pf20::dynamic_extent);
        ASSERT_EQ(improper_subview.size(), TypeParam::extent);
        ASSERT_EQ(improper_subview.size(), improper_subject.size());
        for (std::size_t i = 0; i < improper_subview.size(); ++i)
        {
            ASSERT_EQ(i, improper_subview[i] - 1);
        }

        const std::size_t offset = 1;
        const std::size_t count  = TypeParam::extent - 2;

        TypeParam proper_subject(testData.data(), TypeParam::extent);
        auto      proper_subview = proper_subject.subspan(offset, count);
        ASSERT_EQ(decltype(proper_subview)::extent, cetl::pf20::dynamic_extent);
        ASSERT_EQ(proper_subview.size(), count);
        ASSERT_NE(proper_subview.size(), proper_subject.size());
        for (std::size_t i = 0; i < proper_subview.size(); ++i)
        {
            ASSERT_EQ(i + offset, proper_subview[i] - 1);
        }

        auto proper_subview_dcount = proper_subject.subspan(offset);
        ASSERT_EQ(decltype(proper_subview_dcount)::extent, cetl::pf20::dynamic_extent);
        ASSERT_EQ(proper_subview_dcount.size(), TypeParam::extent - offset);
        ASSERT_NE(proper_subview_dcount.size(), proper_subject.size());
        for (std::size_t i = 0; i < proper_subview_dcount.size(); ++i)
        {
            ASSERT_EQ(i + offset, proper_subview_dcount[i] - 1);
        }
    }
};

template <typename TypeParam>
struct TestSubviewSubspanDynamicByExtent<TypeParam,
                                         std::integral_constant<bool, TypeParam::extent == cetl::pf20::dynamic_extent>>
{
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&& testData)
    {
        constexpr std::size_t data_extent = SpanData<DeducedTypeParam>::data_len;
        TypeParam             improper_subject(testData.data(), data_extent);
        auto                  improper_subview = improper_subject.subspan(0, data_extent);
        ASSERT_EQ(decltype(improper_subview)::extent, cetl::pf20::dynamic_extent);
        ASSERT_EQ(improper_subview.size(), data_extent);
        ASSERT_EQ(improper_subview.size(), improper_subject.size());
        for (std::size_t i = 0; i < improper_subview.size(); ++i)
        {
            ASSERT_EQ(i, improper_subview[i] - 1);
        }

        const std::size_t offset = 1;
        const std::size_t count  = data_extent - 2;

        TypeParam proper_subject(testData.data(), data_extent);
        auto      proper_subview = proper_subject.subspan(offset, count);
        ASSERT_EQ(decltype(proper_subview)::extent, cetl::pf20::dynamic_extent);
        ASSERT_EQ(proper_subview.size(), count);
        ASSERT_NE(proper_subview.size(), proper_subject.size());
        for (std::size_t i = 0; i < proper_subview.size(); ++i)
        {
            ASSERT_EQ(i + offset, proper_subview[i] - 1);
        }

        auto proper_subview_dcount = proper_subject.subspan(offset);
        ASSERT_EQ(decltype(proper_subview_dcount)::extent, cetl::pf20::dynamic_extent);
        ASSERT_EQ(proper_subview_dcount.size(), data_extent - offset);
        ASSERT_NE(proper_subview_dcount.size(), proper_subject.size());
        for (std::size_t i = 0; i < proper_subview_dcount.size(); ++i)
        {
            ASSERT_EQ(i + offset, proper_subview_dcount[i] - 1);
        }
    }
};

template <typename TypeParam>
    struct TestSubviewSubspanDynamicByExtent < TypeParam,
    std::integral_constant<bool, TypeParam::extent<2>>
{
    // nothing to test.
    template <typename DeducedTypeParam>
    void operator()(SpanData<DeducedTypeParam>&&)
    {
    }
};

TYPED_TEST(TestSpan, TestSubviewSubspanDynamic)
{
    TestSubviewSubspanDynamicByExtent<TypeParam> test;
    test(SpanData<TypeParam>());
}

TEST(TestSpanCopyCtor, CopyCetlSpanSame)
{
    int                three[3] = {0, 1, 2};
    cetl::pf20::span<int, 3> fixture(three);
    cetl::pf20::span<int, 3> subject(fixture);

    ASSERT_EQ(fixture.size(), subject.size());
    ASSERT_EQ(fixture.data(), subject.data());
}

TEST(TestSpanCopyCtor, CopyCetlSpanFromDynamic)
{
    int                three[3] = {0, 1, 2};
    cetl::pf20::span<int>    fixture(three, 3);
    cetl::pf20::span<int, 3> subject(fixture);

    ASSERT_EQ(fixture.size(), subject.size());
    ASSERT_EQ(fixture.data(), subject.data());
}

TEST(TestSpanCopyCtor, CopyCetlSpanToDynamic)
{
    int                three[3] = {0, 1, 2};
    cetl::pf20::span<int, 3> fixture(three, 3);
    cetl::pf20::span<int>    subject(fixture);

    ASSERT_EQ(fixture.size(), subject.size());
    ASSERT_EQ(fixture.data(), subject.data());
}

TEST(TestSpanCopyCtor, CopyCetlSpanToFromDynamic)
{
    int             three[3] = {0, 1, 2};
    cetl::pf20::span<int> fixture(three, 3);
    cetl::pf20::span<int> subject(fixture);

    ASSERT_EQ(fixture.size(), subject.size());
    ASSERT_EQ(fixture.data(), subject.data());
}

}  //  namespace
