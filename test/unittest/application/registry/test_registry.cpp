/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "tracking_memory_resource.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/application/registry/register.hpp>
#include <libcyphal/application/registry/register_impl.hpp>
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
using testing::StrEq;
using testing::IsNull;
using testing::IsEmpty;
using testing::Optional;
using testing::ElementsAre;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

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
    EXPECT_THAT(rgy.index(0), IsNull());
    EXPECT_THAT(rgy.get("foo"), Eq(cetl::nullopt));
}

TEST_F(TestRegistry, lifetime)
{
    Registry rgy{mr_};

    EXPECT_THAT(rgy.size(), 0);
    EXPECT_THAT(rgy.index(0), IsNull());
    EXPECT_THAT(rgy.index(1), IsNull());
    EXPECT_THAT(rgy.get("arr"), Eq(cetl::nullopt));
    EXPECT_THAT(rgy.get("bool"), Eq(cetl::nullopt));
    {
        const ParamRegister<std::array<std::int32_t, 3>> r_arr{rgy, "arr", {123, 456, -789}};

        EXPECT_THAT(rgy.size(), 1);
        EXPECT_THAT(rgy.index(0), StrEq("arr"));
        EXPECT_THAT(rgy.index(1), IsNull());
        EXPECT_THAT(rgy.get("arr"), Optional(_));
        EXPECT_THAT(rgy.get("bool"), Eq(cetl::nullopt));
        {
            const ParamRegister<bool> r_bool{rgy, "bool", true};

            EXPECT_THAT(rgy.size(), 2);
            EXPECT_THAT(rgy.index(0), StrEq("arr"));
            EXPECT_THAT(rgy.index(1), StrEq("bool"));
            EXPECT_THAT(rgy.get("arr"), Optional(_));
            EXPECT_THAT(rgy.get("bool"), Optional(_));
            {
                const ParamRegister<double> r_dbl{rgy, "dbl", 1.23};

                EXPECT_THAT(rgy.size(), 3);
                EXPECT_THAT(rgy.index(0), StrEq("arr"));
                EXPECT_THAT(rgy.index(1), StrEq("dbl"));
                EXPECT_THAT(rgy.index(2), StrEq("bool"));
                EXPECT_THAT(rgy.get("arr"), Optional(_));
                EXPECT_THAT(rgy.get("bool"), Optional(_));
                EXPECT_THAT(rgy.get("dbl"), Optional(_));
            }
        }
        EXPECT_THAT(rgy.size(), 1);
        EXPECT_THAT(rgy.index(0), StrEq("arr"));
        EXPECT_THAT(rgy.index(1), IsNull());
        EXPECT_THAT(rgy.get("arr"), Optional(_));
        EXPECT_THAT(rgy.get("bool"), Eq(cetl::nullopt));
    }
    EXPECT_THAT(rgy.size(), 0);
    EXPECT_THAT(rgy.index(0), IsNull());
    EXPECT_THAT(rgy.index(1), IsNull());
    EXPECT_THAT(rgy.get("arr"), Eq(cetl::nullopt));
    EXPECT_THAT(rgy.get("bool"), Eq(cetl::nullopt));
}

TEST_F(TestRegistry, empty_set)
{
    Registry rgy{mr_};

    EXPECT_THAT(rgy.set("foo", Value{alloc_}), Optional(SetError::Existence));
}

TEST_F(TestRegistry, append_set_get_mutable)
{
    Registry rgy{mr_};

    const ParamRegister<std::array<std::int32_t, 3>> r_arr{rgy, "arr", {123, 456, -789}, {false}};
    EXPECT_TRUE(r_arr.isLinked());
    EXPECT_THAT(r_arr.getOptions().persistent, false);
    EXPECT_THAT(rgy.size(), 1);
    EXPECT_THAT(rgy.index(0), StrEq("arr"));

    EXPECT_THAT(rgy.set("arr", makeValue(alloc_, -654.456F)), Eq(cetl::nullopt));  // Coerced to -654.
    const auto arr_get_result = rgy.get("arr");
    ASSERT_THAT(arr_get_result, Optional(_));
    EXPECT_THAT(arr_get_result->flags_.mutable_, true);
    EXPECT_THAT(arr_get_result->flags_.persistent_, false);
    EXPECT_THAT(arr_get_result->value_.is_integer32(), true);
    EXPECT_THAT((get<std::array<std::int32_t, 4>>(arr_get_result->value_)), Optional(ElementsAre(-654, 456, -789, 0)));
}

TEST_F(TestRegistry, append_set_get_immutable)
{
    Registry rgy{mr_};

    const ParamRegister<std::array<std::int32_t, 3>, false> r_arr{rgy, "arr", {123, 456, -789}};
    EXPECT_THAT(r_arr.getOptions().persistent, true);

    EXPECT_THAT(rgy.set("arr", makeValue(alloc_, -654.456F)), Optional(SetError::Mutability));

    const auto arr_get_result = rgy.get("arr");
    ASSERT_THAT(arr_get_result, Optional(_));
    EXPECT_THAT(arr_get_result->flags_.mutable_, false);
    EXPECT_THAT(arr_get_result->flags_.persistent_, true);
    EXPECT_THAT(arr_get_result->value_.is_integer32(), true);
    EXPECT_THAT((get<std::array<std::int32_t, 4>>(arr_get_result->value_)), Optional(ElementsAre(123, 456, -789, 0)));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
