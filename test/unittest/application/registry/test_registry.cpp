/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "tracking_memory_resource.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/application/registry/register.hpp>
#include <libcyphal/application/registry/registry_impl.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <iterator>

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
        // TODO: Uncomment this when std::vector's allocator propagation issue will be resolved.
        // EXPECT_THAT(mr_default_.total_allocated_bytes, 0);
    }

    template <typename Container>
    IRegister::Value makeInt32Value(const Container& container) const
    {
        IRegister::Value value{alloc_};
        auto&            int32 = value.set_integer32();
        std::copy(container.begin(), container.end(), std::back_inserter(int32.value));
        return value;
    }

    IRegister::Value::_traits_::TypeOf::integer32 makeInt32(const std::initializer_list<std::int32_t>& il) const
    {
        IRegister::Value::_traits_::TypeOf::integer32 int32arr{alloc_};
        std::copy(il.begin(), il.end(), std::back_inserter(int32arr.value));
        return int32arr;
    }

    IRegister::Value makeInt32Value(const std::initializer_list<std::int32_t>& il) const
    {
        IRegister::Value value{alloc_};
        auto&            int32 = value.set_integer32();
        std::copy(il.begin(), il.end(), std::back_inserter(int32.value));
        return value;
    }

    IRegister::Value makeStringValue(const cetl::string_view sv) const
    {
        IRegister::Value value{alloc_};
        auto&            str = value.set_string();
        std::copy(sv.begin(), sv.end(), std::back_inserter(str.value));
        return value;
    }

    // MARK: Data members:

    // NOLINTBEGIN
    TrackingMemoryResource           mr_;
    TrackingMemoryResource           mr_default_;
    IRegister::Value::allocator_type alloc_{&mr_};
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

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_F(TestRegistry, lifetime)
{
    Registry rgy{mr_};

    const auto getter = [this] { return IRegister::Value{alloc_}; };
    const auto setter = [](const auto&) { return cetl::nullopt; };

    EXPECT_THAT(rgy.size(), 0);
    EXPECT_THAT(rgy.index(0), IsEmpty());
    EXPECT_THAT(rgy.index(1), IsEmpty());
    EXPECT_THAT(rgy.get("arr"), Eq(cetl::nullopt));
    EXPECT_THAT(rgy.get("bool"), Eq(cetl::nullopt));
    {
        const auto r_arr = rgy.route("arr", getter, setter);

        EXPECT_THAT(rgy.size(), 1);
        EXPECT_THAT(rgy.index(0), "arr");
        EXPECT_THAT(rgy.index(1), IsEmpty());
        EXPECT_THAT(rgy.get("arr"), Optional(_));
        EXPECT_THAT(rgy.set("arr", makeInt32Value({123})), Eq(cetl::nullopt));
        EXPECT_THAT(rgy.get("bool"), Eq(cetl::nullopt));
        {
            const auto r_bool = rgy.route("bool", getter, setter);

            EXPECT_THAT(rgy.size(), 2);
            EXPECT_THAT(rgy.index(0), "arr");
            EXPECT_THAT(rgy.index(1), "bool");
            EXPECT_THAT(rgy.get("arr"), Optional(_));
            EXPECT_THAT(rgy.get("bool"), Optional(_));
            {
                const auto r_dbl = rgy.route("dbl", getter, setter);

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

    EXPECT_THAT(rgy.set("foo", IRegister::Value{alloc_}), Optional(SetError::Existence));
}

TEST_F(TestRegistry, route_mutable)
{
    Registry rgy{mr_};

    std::array<std::int32_t, 3> v_arr{123, 456, -789};

    const auto r_arr = rgy.route(
        "arr",
        [this, &v_arr] { return makeInt32Value(v_arr); },
        [&v_arr](const IRegister::Value& v) -> cetl::optional<SetError> {
            //
            if (const auto* const int32 = v.get_integer32_if())
            {
                std::copy_n(int32->value.begin(), std::min(int32->value.size(), v_arr.size()), v_arr.begin());
                return cetl::nullopt;
            }
            return SetError::Semantics;
        },
        {true});
    EXPECT_TRUE(r_arr.isLinked());
    EXPECT_THAT(rgy.size(), 1);
    EXPECT_THAT(rgy.index(0), "arr");
    EXPECT_THAT(v_arr, ElementsAre(123, 456, -789));

    EXPECT_THAT(rgy.set("arr", makeInt32Value({-654})), Eq(cetl::nullopt));
    EXPECT_THAT(rgy.set("arr", makeStringValue("bad")), Optional(SetError::Semantics));
    const auto arr_get_result = rgy.get("arr");
    ASSERT_TRUE(arr_get_result);
    EXPECT_THAT(arr_get_result->flags._mutable, true);
    EXPECT_THAT(arr_get_result->flags.persistent, true);
    ASSERT_THAT(arr_get_result->value.is_integer32(), true);
    EXPECT_THAT(arr_get_result->value.get_integer32().value, ElementsAre(-654, 456, -789));
    EXPECT_THAT(v_arr, ElementsAre(-654, 456, -789));

    // The same name failure!
    //
    IRegister::Value same_reg_value{alloc_};
    auto             same_reg = rgy.route(
        "arr",
        [&same_reg_value] { return same_reg_value; },
        [&same_reg_value](const auto& new_value) {
            //
            same_reg_value = new_value;
            return cetl::nullopt;
        });
    EXPECT_FALSE(same_reg.isLinked());
    // Despite the failure, the register should still work (be gettable/settable).
    EXPECT_THAT(same_reg.set(makeInt32Value({147})), Eq(cetl::nullopt));
    const auto same_reg_result = same_reg.get();
    EXPECT_THAT(same_reg_result.flags._mutable, true);
    EXPECT_THAT(same_reg_result.flags.persistent, false);
    ASSERT_THAT(same_reg_result.value.is_integer32(), true);
    EXPECT_THAT(same_reg_result.value.get_integer32().value, ElementsAre(147));
    ASSERT_THAT(same_reg_value.is_integer32(), true);
    EXPECT_THAT(same_reg_value.get_integer32().value, ElementsAre(147));
}

TEST_F(TestRegistry, route_immutable)
{
    Registry rgy{mr_};

    const auto r_arr = rgy.route("arr", [this] { return makeInt32({123, 456, -789}); });
    EXPECT_TRUE(r_arr.isLinked());
    EXPECT_THAT(rgy.size(), 1);
    EXPECT_THAT(rgy.index(0), "arr");

    EXPECT_THAT(rgy.set("arr", makeInt32Value({-654})), Optional(SetError::Mutability));
    const auto arr_get_result = rgy.get("arr");
    ASSERT_TRUE(arr_get_result);
    EXPECT_THAT(arr_get_result->flags._mutable, false);
    EXPECT_THAT(arr_get_result->flags.persistent, false);
    ASSERT_THAT(arr_get_result->value.is_integer32(), true);
    EXPECT_THAT(arr_get_result->value.get_integer32().value, ElementsAre(123, 456, -789));

    // The same name failure!
    //
    auto same_reg_value = makeInt32Value({147});
    auto same_reg       = rgy.route("arr", [&same_reg_value] { return same_reg_value; });
    EXPECT_FALSE(same_reg.isLinked());
    // Despite the failure, the register should still work (be gettable/settable).
    EXPECT_THAT(same_reg.set(makeInt32Value({13})), Optional(SetError::Mutability));
    const auto same_reg_result = same_reg.get();
    EXPECT_THAT(same_reg_result.flags._mutable, false);
    EXPECT_THAT(same_reg_result.flags.persistent, false);
    ASSERT_THAT(same_reg_result.value.is_integer32(), true);
    EXPECT_THAT(same_reg_result.value.get_integer32().value, ElementsAre(147));
    ASSERT_THAT(same_reg_value.is_integer32(), true);
    EXPECT_THAT(same_reg_value.get_integer32().value, ElementsAre(147));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers, bugprone-unchecked-optional-access)

}  // namespace
