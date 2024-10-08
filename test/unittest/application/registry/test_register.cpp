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
#include <utility>

namespace
{

using namespace libcyphal::application::registry;  // NOLINT This our main concern here in the unit tests.

using testing::Eq;
using testing::IsEmpty;
using testing::Optional;
using testing::ElementsAre;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestRegister : public testing::Test
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

};  // TestRegister

// MARK: - Tests:

TEST_F(TestRegister, paramReg_set_get_mutable)
{
    ParamRegister<std::array<std::int32_t, 3>> r_arr{mr_, "arr", {123, 456, -789}};
    EXPECT_THAT(r_arr.getOptions().persistent, true);

    EXPECT_FALSE(r_arr.isLinked());
    EXPECT_THAT(r_arr.set(makeValue(alloc_, -654.456F)), Eq(cetl::nullopt));  // Coerced to -654.
    EXPECT_THAT(r_arr.get().flags_.mutable_, true);
    EXPECT_THAT(r_arr.get().flags_.persistent_, true);
    EXPECT_THAT(r_arr.get().value_.is_integer32(), true);
    EXPECT_THAT((get<std::array<std::int32_t, 4>>(r_arr.get().value_)), Optional(ElementsAre(-654, 456, -789, 0)));
}

TEST_F(TestRegister, paramReg_set_get_immutable)
{
    Registry rgy{mr_};

    ParamRegister<std::array<std::int32_t, 3>, false> r_arr{rgy, "arr", {123, 456, -789}, {false}};
    EXPECT_TRUE(r_arr.isLinked());
    EXPECT_THAT(r_arr.getOptions().persistent, false);

    EXPECT_THAT(r_arr.set(makeValue(alloc_, -654.456F)), Optional(SetError::Mutability));

    EXPECT_THAT(r_arr.get().flags_.mutable_, false);
    EXPECT_THAT(r_arr.get().flags_.persistent_, false);
    EXPECT_THAT(r_arr.get().value_.is_integer32(), true);
    EXPECT_THAT((get<std::array<std::int32_t, 4>>(r_arr.get().value_)), Optional(ElementsAre(123, 456, -789, 0)));
}

TEST_F(TestRegister, paramReg_set_move_get)
{
    Registry rgy{mr_};

    ParamRegister<std::array<std::int32_t, 3>> r_arr1{rgy, "arr", {123, 456, -789}, {false}};
    EXPECT_TRUE(r_arr1.isLinked());
    EXPECT_THAT(r_arr1.getOptions().persistent, false);
    EXPECT_THAT(r_arr1.set(makeValue(alloc_, false)), Eq(cetl::nullopt));  // Coerced to 0.

    const ParamRegister<std::array<std::int32_t, 3>> r_arr2{std::move(r_arr1)};
    EXPECT_TRUE(r_arr2.isLinked());
    EXPECT_THAT(r_arr2.getOptions().persistent, false);

    EXPECT_THAT(r_arr2.get().flags_.mutable_, true);
    EXPECT_THAT(r_arr2.get().flags_.persistent_, false);
    EXPECT_THAT(r_arr2.get().value_.is_integer32(), true);
    EXPECT_THAT((get<std::array<std::int32_t, 4>>(r_arr2.get().value_)), Optional(ElementsAre(0, 456, -789, 0)));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
