/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "tracking_memory_resource.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/application/registry/register.hpp>
#include <libcyphal/application/registry/register_impl.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>

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

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers, bugprone-unchecked-optional-access)

class TestRegister : public testing::Test
{
protected:
    class AccessorsMock
    {
    public:
        IRegister::Value operator()() const
        {
            return getter();
        }
        cetl::optional<SetError> operator()(const IRegister::Value& value)
        {
            return setter(value);
        }
        MOCK_METHOD(IRegister::Value, getter, (), (const));
        MOCK_METHOD(cetl::optional<SetError>, setter, (const IRegister::Value&), ());

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

    IRegister::Value makeBitValue(const std::initializer_list<bool>& il) const
    {
        IRegister::Value value{alloc_};
        auto&            bits = value.set_bit();
        std::copy(il.begin(), il.end(), std::back_inserter(bits.value));
        return value;
    }

    IRegister::Value makeInt32Value(const std::initializer_list<std::int32_t>& il) const
    {
        IRegister::Value value{alloc_};
        auto&            int32 = value.set_integer32();
        std::copy(il.begin(), il.end(), std::back_inserter(int32.value));
        return value;
    }

    // MARK: Data members:

    // NOLINTBEGIN
    TrackingMemoryResource           mr_;
    TrackingMemoryResource           mr_default_;
    IRegister::Value::allocator_type alloc_{&mr_};
    // NOLINTEND

};  // TestRegister

// MARK: - Tests:

TEST_F(TestRegister, makeRegister_set_get_immutable)
{
    StrictMock<AccessorsMock> accessors_mock;

    auto r_bool = makeRegister(mr_, "bool", std::ref(accessors_mock));
    EXPECT_FALSE(r_bool.isLinked());

    EXPECT_THAT(r_bool.set(IRegister::Value{alloc_}), Optional(SetError::Mutability));

    EXPECT_CALL(accessors_mock, getter()).WillOnce(Return(makeBitValue({true, false, true})));
    const auto result = r_bool.get();
    EXPECT_THAT(result.flags._mutable, false);
    EXPECT_THAT(result.flags.persistent, false);
    ASSERT_THAT(result.value.is_bit(), true);
    EXPECT_THAT(result.value.get_bit().value, ElementsAre(true, false, true));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_F(TestRegister, makeRegister_set_get_mutable)
{
    StrictMock<AccessorsMock> accessors_mock;

    auto r_bool = makeRegister(mr_, "bool", std::ref(accessors_mock), std::ref(accessors_mock));
    EXPECT_FALSE(r_bool.isLinked());

    // 1st set
    {
        EXPECT_CALL(accessors_mock, setter(_))  //
            .WillOnce(testing::Invoke([](const IRegister::Value& value) {
                //
                EXPECT_THAT(value.is_bit(), true);
                EXPECT_THAT(value.get_bit().value, ElementsAre(true, true, false));
                return cetl::nullopt;
            }));
        EXPECT_THAT(r_bool.set(makeBitValue({true, true, false})), Eq(cetl::nullopt));

        EXPECT_CALL(accessors_mock, getter()).WillOnce(Return(makeBitValue({true, true, false})));
        const auto result = r_bool.get();
        EXPECT_THAT(result.flags._mutable, true);
        EXPECT_THAT(result.flags.persistent, false);
        ASSERT_THAT(result.value.is_bit(), true);
        EXPECT_THAT(result.value.get_bit().value, ElementsAre(true, true, false));
    }
    // 2nd set
    {
        EXPECT_CALL(accessors_mock, setter(_))  //
            .WillOnce(testing::Invoke([](const IRegister::Value& value) {
                //
                EXPECT_THAT(value.is_integer32(), true);
                EXPECT_THAT(value.get_integer32().value, ElementsAre(1, 2, 3));
                return cetl::nullopt;
            }));
        EXPECT_THAT(r_bool.set(makeInt32Value({1, 2, 3})), Eq(cetl::nullopt));

        EXPECT_CALL(accessors_mock, getter()).WillOnce(Return(makeInt32Value({1, 2, 3})));
        const auto result = r_bool.get();
        EXPECT_THAT(result.flags._mutable, true);
        EXPECT_THAT(result.flags.persistent, false);
        EXPECT_THAT(result.value.is_bit(), false);
        ASSERT_THAT(result.value.is_integer32(), true);
        EXPECT_THAT(result.value.get_integer32().value, ElementsAre(1, 2, 3));
    }
}

TEST_F(TestRegister, makeRegister_set_failure)
{
    StrictMock<AccessorsMock> accessors_mock;

    auto r_int32 = makeRegister(mr_, "int32", std::ref(accessors_mock), std::ref(accessors_mock));
    EXPECT_FALSE(r_int32.isLinked());

    EXPECT_CALL(accessors_mock, setter(_)).WillOnce(Return(SetError::Semantics));  // Failure!
    EXPECT_THAT(r_int32.set(makeInt32Value({13})), Optional(SetError::Semantics));

    EXPECT_CALL(accessors_mock, setter(_)).WillOnce(Return(SetError::Semantics));  // Failure!
    EXPECT_THAT(r_int32.set(makeInt32Value({13})), Optional(SetError::Semantics));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers, bugprone-unchecked-optional-access)

}  // namespace
