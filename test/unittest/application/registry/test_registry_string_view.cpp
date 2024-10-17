/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <libcyphal/application/registry/registry_string_view.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <type_traits>

namespace
{

using namespace libcyphal::application::registry;  // NOLINT This our main concern here in the unit tests.

using testing::Not;
using testing::StrEq;
using testing::IsNull;
using testing::SizeIs;
using testing::IsEmpty;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestRegistryStringView : public testing::Test
{
    static_assert(std::is_nothrow_default_constructible<StringView>::value,  //
                  "StringView must be default constructible");
    static_assert(std::is_nothrow_copy_constructible<StringView>::value,  //
                  "StringView must be copy constructible");
    static_assert(std::is_nothrow_move_constructible<StringView>::value,  //
                  "StringView must be move constructible");
    static_assert(std::is_nothrow_copy_assignable<StringView>::value,  //
                  "StringView must be copy assignable");
    static_assert(std::is_nothrow_move_assignable<StringView>::value,  //
                  "StringView must be move assignable");
    static_assert(std::is_convertible<const char*, StringView>::value,  //
                  "StringView must be convertible from const char*");
    static_assert(!std::is_constructible<StringView, std::nullptr_t>::value,  //
                  "StringView must not be constructible from nullptr_t");

};  // TestRegistryStringView

// MARK: - Tests:

TEST_F(TestRegistryStringView, ctor_1)
{
    constexpr StringView sv{};
    EXPECT_THAT(sv, IsEmpty());
    EXPECT_THAT(sv, SizeIs(0));
    EXPECT_THAT(sv.data(), IsNull());
}

TEST_F(TestRegistryStringView, ctor_2)
{
    const StringView sv1{"abc"};

    const StringView sv2{sv1};
    EXPECT_FALSE(sv2.empty());
    EXPECT_THAT(sv2, SizeIs(3));
    EXPECT_THAT(sv2.data(), StrEq("abc"));
}

TEST_F(TestRegistryStringView, ctor_3)
{
    constexpr StringView sv1{"abcdef", 3};
    EXPECT_FALSE(sv1.empty());
    EXPECT_THAT(sv1, SizeIs(3));
    EXPECT_THAT(sv1.data(), StrEq("abcdef"));

    constexpr StringView sv2{nullptr, 0};
    EXPECT_THAT(sv2, IsEmpty());
    EXPECT_THAT(sv2, SizeIs(0));
    EXPECT_THAT(sv2.data(), IsNull());
}

TEST_F(TestRegistryStringView, ctor_4)
{
    const StringView sv1 = "abcdef";
    EXPECT_FALSE(sv1.empty());
    EXPECT_THAT(sv1, SizeIs(6));
    EXPECT_THAT(sv1.data(), StrEq("abcdef"));

    const StringView sv2{static_cast<const char*>(nullptr)};
    EXPECT_THAT(sv2, IsEmpty());
    EXPECT_THAT(sv2, SizeIs(0));
    EXPECT_THAT(sv2.data(), IsNull());
}

TEST_F(TestRegistryStringView, operator_eq)
{
    EXPECT_THAT(StringView{}, "");
    EXPECT_THAT(StringView{""}, "");
    EXPECT_THAT(StringView{""}, Not(" "));
    EXPECT_THAT(StringView{"abc"}, "abc");
    EXPECT_THAT(StringView{"abc"}, Not("aBc"));
    EXPECT_THAT(StringView{"abc"}, Not("abcd"));
    EXPECT_THAT(StringView{"abcd"}, Not("abc"));
    EXPECT_THAT(StringView{"abc"}, (StringView{"abcdef", 3}));
    EXPECT_THAT(StringView{"abc"}, Not(StringView{"abcdef", 4}));

    const auto* const null_str = static_cast<const char*>(nullptr);
    EXPECT_THAT(StringView{""}, null_str);
    EXPECT_THAT(StringView{null_str}, null_str);
    EXPECT_THAT(StringView{"abc"}, Not(null_str));
    EXPECT_THAT(StringView{null_str}, Not("abc"));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
