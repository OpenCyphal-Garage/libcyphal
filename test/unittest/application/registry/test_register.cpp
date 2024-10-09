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
#include <functional>
#include <utility>

namespace
{

using namespace libcyphal::application::registry;  // NOLINT This our main concern here in the unit tests.

using testing::_;
using testing::Eq;
using testing::Return;
using testing::IsEmpty;
using testing::Optional;
using testing::StrictMock;
using testing::ElementsAre;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestRegister : public testing::Test
{
protected:
    class AccessorsMock
    {
    public:
        Value operator()() const
        {
            return getter();
        }
        bool operator()(const Value& value)
        {
            return setter(value);
        }
        MOCK_METHOD(Value, getter, (), (const));
        MOCK_METHOD(bool, setter, (const Value&), ());

    };  // AccessorMock

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
        // TODO: Uncomment this when std::vector's allocator propagation issue will be resolved.
        // EXPECT_THAT(mr_default_.total_allocated_bytes, 0);
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
    ParamRegister<std::array<std::int32_t, 3>> r_arr{mr_, "arr", {123, 456, -789}, {true}};
    EXPECT_THAT(r_arr.getOptions().persistent, true);

    EXPECT_FALSE(r_arr.isLinked());
    EXPECT_THAT(r_arr.set(makeValue(alloc_, -654.456F)), Eq(cetl::nullopt));  // Coerced to -654.
    EXPECT_THAT(r_arr.get().flags_.mutable_, true);
    EXPECT_THAT(r_arr.get().flags_.persistent_, true);
    EXPECT_THAT(r_arr.get().value_.is_integer32(), true);
    EXPECT_THAT((get<std::array<std::int32_t, 4>>(r_arr.get().value_)), Optional(ElementsAre(-654, 456, -789, 0)));
}

TEST_F(TestRegister, exposeParam_set_get_immutable)
{
    Registry rgy{mr_};

    auto r_arr = rgy.exposeParam<std::array<std::int32_t, 3>, false>("arr", {123, 456, -789});
    ASSERT_THAT(r_arr, Optional(_));
    EXPECT_TRUE(r_arr->isLinked());
    EXPECT_THAT(r_arr->getOptions().persistent, false);

    EXPECT_THAT(r_arr->set(makeValue(alloc_, -654.456F)), Optional(SetError::Mutability));

    EXPECT_THAT(r_arr->get().flags_.mutable_, false);
    EXPECT_THAT(r_arr->get().flags_.persistent_, false);
    EXPECT_THAT(r_arr->get().value_.is_integer32(), true);
    EXPECT_THAT((get<std::array<std::int32_t, 4>>(r_arr->get().value_)), Optional(ElementsAre(123, 456, -789, 0)));
}

TEST_F(TestRegister, exposeParam_set_move_get)
{
    Registry rgy{mr_};

    auto r_arr1 = rgy.exposeParam("arr", std::array<std::int32_t, 3>{123, 456, -789});
    ASSERT_THAT(r_arr1, Optional(_));
    EXPECT_TRUE(r_arr1->isLinked());
    EXPECT_THAT(r_arr1->getOptions().persistent, false);
    EXPECT_THAT(r_arr1->set(makeValue(alloc_, false)), Eq(cetl::nullopt));  // Coerced to 0.

    const auto r_arr2{std::move(r_arr1.value())};
    EXPECT_TRUE(r_arr2.isLinked());
    EXPECT_THAT(r_arr2.getOptions().persistent, false);

    EXPECT_THAT(r_arr2.get().flags_.mutable_, true);
    EXPECT_THAT(r_arr2.get().flags_.persistent_, false);
    EXPECT_THAT(r_arr2.get().value_.is_integer32(), true);
    EXPECT_THAT((get<std::array<std::int32_t, 4>>(r_arr2.get().value_)), Optional(ElementsAre(0, 456, -789, 0)));
}

TEST_F(TestRegister, paramReg_set_failure)
{
    ParamRegister<bool> r_bool{mr_, "bool", true};
    EXPECT_THAT(r_bool.set(makeValue(alloc_, "xxx")), Optional(SetError::Coercion));
}

TEST_F(TestRegister, makeRegister_set_get_immutable)
{
    StrictMock<AccessorsMock> accessors_mock;

    auto r_bool = makeRegister(mr_, "bool", {}, std::ref(accessors_mock));
    EXPECT_FALSE(r_bool.isLinked());
    EXPECT_THAT(r_bool.getOptions().persistent, false);

    EXPECT_THAT(r_bool.set(makeValue(alloc_, 1.0, 0.0)), Optional(SetError::Mutability));

    EXPECT_CALL(accessors_mock, getter()).WillOnce(Return(makeValue(alloc_, true)));
    const auto result = r_bool.get();
    EXPECT_THAT(result.flags_.mutable_, false);
    EXPECT_THAT(result.flags_.persistent_, false);
    EXPECT_THAT(result.value_.is_bit(), true);
    EXPECT_THAT((get<std::array<bool, 3>>(result.value_)), Optional(ElementsAre(true, false, false)));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_F(TestRegister, makeRegister_set_get_mutable)
{
    StrictMock<AccessorsMock> accessors_mock;

    auto r_bool = makeRegister(mr_, "bool", {}, std::ref(accessors_mock), std::ref(accessors_mock));
    EXPECT_FALSE(r_bool.isLinked());
    EXPECT_THAT(r_bool.getOptions().persistent, false);

    // 1st set
    {
        EXPECT_CALL(accessors_mock, getter()).WillOnce(Return(makeValue(alloc_, true, false)));
        EXPECT_CALL(accessors_mock, setter(_))  //
            .WillOnce(testing::Invoke([](const Value& value) {
                //
                EXPECT_THAT(value.is_bit(), true);
                EXPECT_THAT(value.get_bit().value.size(), 2);
                EXPECT_THAT(value.get_bit().value[0], false);
                EXPECT_THAT(value.get_bit().value[1], true);
                return true;
            }));
        EXPECT_THAT(r_bool.set(makeValue(alloc_, 0.0, 1.0, 2.0)), Eq(cetl::nullopt));

        EXPECT_CALL(accessors_mock, getter()).WillOnce(Return(makeValue(alloc_, false, true)));
        const auto result = r_bool.get();
        EXPECT_THAT(result.flags_.mutable_, true);
        EXPECT_THAT(result.flags_.persistent_, false);
        EXPECT_THAT(result.value_.is_bit(), true);
        EXPECT_THAT((get<std::array<bool, 3>>(result.value_)), Optional(ElementsAre(false, true, false)));
    }
    // 2nd set
    {
        EXPECT_CALL(accessors_mock, getter()).WillOnce(Return(makeValue(alloc_, false, true)));
        EXPECT_CALL(accessors_mock, setter(_))  //
            .WillOnce(testing::Invoke([](const Value& value) {
                //
                EXPECT_THAT(value.is_bit(), true);
                EXPECT_THAT(value.get_bit().value.size(), 2);
                EXPECT_THAT(value.get_bit().value[0], true);
                EXPECT_THAT(value.get_bit().value[1], true);
                return true;
            }));
        EXPECT_THAT(r_bool.set(makeValue(alloc_, 1)), Eq(cetl::nullopt));

        EXPECT_CALL(accessors_mock, getter()).WillOnce(Return(makeValue(alloc_, true, true)));
        const auto result = r_bool.get();
        EXPECT_THAT(result.flags_.mutable_, true);
        EXPECT_THAT(result.flags_.persistent_, false);
        EXPECT_THAT(result.value_.is_bit(), true);
        EXPECT_THAT((get<std::array<bool, 3>>(result.value_)), Optional(ElementsAre(true, true, false)));
    }
}

TEST_F(TestRegister, makeRegister_set_failure)
{
    StrictMock<AccessorsMock> accessors_mock;

    auto r_int32 = makeRegister(mr_, "int32", {}, std::ref(accessors_mock), std::ref(accessors_mock));
    EXPECT_FALSE(r_int32.isLinked());
    EXPECT_THAT(r_int32.getOptions().persistent, false);

    EXPECT_CALL(accessors_mock, getter()).WillOnce(Return(makeValue(alloc_, 13)));
    EXPECT_CALL(accessors_mock, setter(_)).WillOnce(Return(false));  // Failure!
    EXPECT_THAT(r_int32.set(makeValue(alloc_, 147)), Optional(SetError::Semantics));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
