/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "tracking_memory_resource.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/application/registry/register.hpp>
#include <libcyphal/application/registry/registry_impl.hpp>
#include <libcyphal/application/registry/registry_value.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>

namespace
{

using namespace libcyphal::application::registry;  // NOLINT This our main concern here in the unit tests.

using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::Optional;
using testing::ElementsAre;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers, bugprone-unchecked-optional-access)

class TestRegistry : public testing::Test
{
protected:
    void SetUp() override
    {
        cetl::pmr::set_default_resource(&mr_default_);
    }

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);

        EXPECT_THAT(mr_default_.allocations, IsEmpty());
        EXPECT_THAT(mr_default_.total_allocated_bytes, mr_default_.total_deallocated_bytes);
        EXPECT_THAT(mr_default_.total_allocated_bytes, 0);
    }

    // MARK: Data members:

    // NOLINTBEGIN
    TrackingMemoryResource mr_;
    TrackingMemoryResource mr_default_;
    Value::allocator_type  alloc_{&mr_};
    // NOLINTEND

};  // TestRegistry

// MARK: - Tests:

TEST_F(TestRegistry, empty)
{
    const Registry rgy{mr_};

    // Ensure empty.
    EXPECT_THAT(rgy.size(), 0);
    EXPECT_THAT(rgy.index(0), IsEmpty());
    EXPECT_THAT(rgy.get("foo"), Eq(cetl::nullopt));
}

TEST_F(TestRegistry, lifetime)
{
    Registry rgy{mr_};

    EXPECT_THAT(rgy.size(), 0);
    EXPECT_THAT(rgy.index(0), IsEmpty());
    EXPECT_THAT(rgy.index(1), IsEmpty());
    EXPECT_THAT(rgy.get("arr"), Eq(cetl::nullopt));
    EXPECT_THAT(rgy.get("bool"), Eq(cetl::nullopt));
    {
        const auto r_arr = rgy.parameterize<std::array<std::int32_t, 3>>("arr", {123, 456, -789});

        EXPECT_THAT(rgy.size(), 1);
        EXPECT_THAT(rgy.index(0), "arr");
        EXPECT_THAT(rgy.index(1), IsEmpty());
        EXPECT_THAT(rgy.get("arr"), Optional(_));
        EXPECT_THAT(rgy.get("bool"), Eq(cetl::nullopt));
        {
            const auto r_bool = rgy.parameterize<bool>("bool", true);

            EXPECT_THAT(rgy.size(), 2);
            EXPECT_THAT(rgy.index(0), "arr");
            EXPECT_THAT(rgy.index(1), "bool");
            EXPECT_THAT(rgy.get("arr"), Optional(_));
            EXPECT_THAT(rgy.get("bool"), Optional(_));
            {
                const auto r_dbl = rgy.parameterize<double>("dbl", 1.23);

                EXPECT_THAT(rgy.size(), 3);
                EXPECT_THAT(rgy.index(0), "arr");
                EXPECT_THAT(rgy.index(1), "dbl");
                EXPECT_THAT(rgy.index(2), "bool");
                EXPECT_THAT(rgy.get("arr"), Optional(_));
                EXPECT_THAT(rgy.get("bool"), Optional(_));
                EXPECT_THAT(rgy.get("dbl"), Optional(_));
            }
        }
        EXPECT_THAT(rgy.size(), 1);
        EXPECT_THAT(rgy.index(0), "arr");
        EXPECT_THAT(rgy.index(1), IsEmpty());
        EXPECT_THAT(rgy.get("arr"), Optional(_));
        EXPECT_THAT(rgy.get("bool"), Eq(cetl::nullopt));
    }
    EXPECT_THAT(rgy.size(), 0);
    EXPECT_THAT(rgy.index(0), IsEmpty());
    EXPECT_THAT(rgy.index(1), IsEmpty());
    EXPECT_THAT(rgy.get("arr"), Eq(cetl::nullopt));
    EXPECT_THAT(rgy.get("bool"), Eq(cetl::nullopt));
}

TEST_F(TestRegistry, empty_set)
{
    Registry rgy{mr_};

    EXPECT_THAT(rgy.set("foo", Value{alloc_}), Optional(SetError::Existence));
}

TEST_F(TestRegistry, route_mutable)
{
    Registry rgy{mr_};

    std::array<std::int32_t, 3> v_arr{123, 456, -789};
    const auto                  r_arr = rgy.route(
        "arr",
        [&v_arr] { return v_arr; },
        [&v_arr](const Value& v) {
            v_arr = get<std::array<std::int32_t, 3>>(v).value();
            return true;
        },
        {true});
    EXPECT_TRUE(r_arr.isLinked());
    EXPECT_THAT(rgy.size(), 1);
    EXPECT_THAT(rgy.index(0), "arr");
    EXPECT_THAT(v_arr, ElementsAre(123, 456, -789));

    EXPECT_THAT(rgy.set("arr", makeValue(alloc_, -654.456F)), Eq(cetl::nullopt));  // Coerced to -654.
    const auto arr_get_result = rgy.get("arr");
    ASSERT_TRUE(arr_get_result);
    EXPECT_THAT(arr_get_result->flags._mutable, true);
    EXPECT_THAT(arr_get_result->flags.persistent, true);
    EXPECT_THAT(arr_get_result->value.is_integer32(), true);
    EXPECT_THAT((get<std::array<std::int32_t, 4>>(arr_get_result->value)), Optional(ElementsAre(-654, 456, -789, 0)));
    EXPECT_THAT(v_arr, ElementsAre(-654, 456, -789));

    // The same name failure!
    EXPECT_FALSE(rgy.route("arr", [] { return true; }, [](const auto&) { return true; }).isLinked());
}

TEST_F(TestRegistry, route_immutable)
{
    Registry rgy{mr_};

    constexpr std::array<std::int32_t, 3> v_arr{123, 456, -789};
    const auto                            r_arr = rgy.route("arr", [&v_arr] { return v_arr; });
    EXPECT_TRUE(r_arr.isLinked());
    EXPECT_THAT(rgy.size(), 1);
    EXPECT_THAT(rgy.index(0), "arr");

    EXPECT_THAT(rgy.set("arr", makeValue(alloc_, -654.456F)), Optional(SetError::Mutability));
    const auto arr_get_result = rgy.get("arr");
    ASSERT_TRUE(arr_get_result);
    EXPECT_THAT(arr_get_result->flags._mutable, false);
    EXPECT_THAT(arr_get_result->flags.persistent, false);
    EXPECT_THAT(arr_get_result->value.is_integer32(), true);
    EXPECT_THAT((get<std::array<std::int32_t, 4>>(arr_get_result->value)), Optional(ElementsAre(123, 456, -789, 0)));

    // The same name failure!
    EXPECT_FALSE(rgy.route("arr", [] { return true; }).isLinked());
}

TEST_F(TestRegistry, expose)
{
    Registry rgy{mr_};

    std::array<std::int32_t, 3> v_arr{123, 456, -789};
    const auto                  r_arr = rgy.expose("arr", v_arr);
    EXPECT_TRUE(r_arr.isLinked());
    EXPECT_THAT(rgy.size(), 1);
    EXPECT_THAT(rgy.index(0), "arr");
    EXPECT_THAT(v_arr, ElementsAre(123, 456, -789));

    EXPECT_THAT(rgy.set("arr", makeValue(alloc_, -654.456F)), Eq(cetl::nullopt));  // Coerced to -654.
    const auto arr_get_result = rgy.get("arr");
    ASSERT_TRUE(arr_get_result);
    EXPECT_THAT(arr_get_result->flags._mutable, true);
    EXPECT_THAT(arr_get_result->flags.persistent, false);
    EXPECT_THAT(arr_get_result->value.is_integer32(), true);
    EXPECT_THAT((get<std::array<std::int32_t, 4>>(arr_get_result->value)), Optional(ElementsAre(-654, 456, -789, 0)));
    EXPECT_THAT(v_arr, ElementsAre(-654, 456, -789));
}

TEST_F(TestRegistry, exposeParam_set_get_mutable)
{
    Registry rgy{mr_};

    const auto r_arr = rgy.parameterize<std::array<std::int32_t, 3>>("arr", {123, 456, -789});
    EXPECT_TRUE(r_arr.isLinked());
    EXPECT_THAT(rgy.size(), 1);
    EXPECT_THAT(rgy.index(0), "arr");

    EXPECT_THAT(rgy.set("arr", makeValue(alloc_, -654.456F)), Eq(cetl::nullopt));  // Coerced to -654.
    const auto arr_get_result = rgy.get("arr");
    ASSERT_TRUE(arr_get_result);
    EXPECT_THAT(arr_get_result->flags._mutable, true);
    EXPECT_THAT(arr_get_result->flags.persistent, false);
    EXPECT_THAT(arr_get_result->value.is_integer32(), true);
    EXPECT_THAT((get<std::array<std::int32_t, 4>>(arr_get_result->value)), Optional(ElementsAre(-654, 456, -789, 0)));
}

TEST_F(TestRegistry, exposeParam_set_get_immutable)
{
    Registry rgy{mr_};

    const auto r_arr = rgy.parameterize<std::array<std::int32_t, 3>, false>("arr", {123, 456, -789}, {true});
    EXPECT_TRUE(r_arr.isLinked());

    EXPECT_THAT(rgy.set("arr", makeValue(alloc_, -654.456F)), Optional(SetError::Mutability));

    const auto arr_get_result = rgy.get("arr");
    ASSERT_TRUE(arr_get_result);
    EXPECT_THAT(arr_get_result->flags._mutable, false);
    EXPECT_THAT(arr_get_result->flags.persistent, true);
    EXPECT_THAT(arr_get_result->value.is_integer32(), true);
    EXPECT_THAT((get<std::array<std::int32_t, 4>>(arr_get_result->value)), Optional(ElementsAre(123, 456, -789, 0)));
}

TEST_F(TestRegistry, exposeParam_failure)
{
    Registry rgy{mr_};

    const auto r_bool1 = rgy.parameterize<bool>("bool", false);
    EXPECT_TRUE(r_bool1.isLinked());

    const auto r_bool2 = rgy.parameterize<bool>("bool", false);  // The same name!
    EXPECT_FALSE(r_bool2.isLinked());
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers, bugprone-unchecked-optional-access)

}  // namespace
