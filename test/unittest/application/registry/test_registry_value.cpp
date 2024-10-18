/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "tracking_memory_resource.hpp"
#include "verification_utilities.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/application/registry/registry_value.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>

namespace
{

using namespace libcyphal::application::registry;  // NOLINT This our main concern here in the unit tests.
using libcyphal::verification_utilities::b;

using testing::SizeIs;
using testing::IsEmpty;
using testing::Optional;
using testing::DoubleNear;
using testing::StartsWith;
using testing::ElementsAre;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestRegistryValue : public testing::Test
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

    // MARK: Data members:

    // NOLINTBEGIN
    TrackingMemoryResource      mr_;
    TrackingMemoryResource      mr_default_;
    const Value::allocator_type alloc_{&mr_};
    // NOLINTEND

};  // TestRegistryValue

// MARK: - Tests:

TEST_F(TestRegistryValue, makeValue)
{
    // Integral
    {
        Value v = makeValue(alloc_, true, false, true, false, false, false, false, true, false);
        EXPECT_TRUE(v.is_bit());
        EXPECT_THAT(v.get_bit().value.size(), 9);
        EXPECT_TRUE(v.get_bit().value[0]);
        EXPECT_FALSE(v.get_bit().value[1]);
        EXPECT_TRUE(v.get_bit().value[2]);
        EXPECT_FALSE(v.get_bit().value[3]);
        EXPECT_FALSE(v.get_bit().value[4]);
        EXPECT_FALSE(v.get_bit().value[5]);
        EXPECT_FALSE(v.get_bit().value[6]);
        EXPECT_TRUE(v.get_bit().value[7]);
        EXPECT_FALSE(v.get_bit().value[8]);

        v = makeValue(alloc_, static_cast<std::int64_t>(-1234567890), 123, 1234567890123);
        EXPECT_TRUE(v.is_integer64());
        ASSERT_THAT(v.get_integer64().value.size(), 3);
        EXPECT_THAT(v.get_integer64().value[0], -1234567890);
        EXPECT_THAT(v.get_integer64().value[1], 123);
        EXPECT_THAT(v.get_integer64().value[2], 1234567890123);

        v = makeValue(alloc_, -123456789, 66);
        EXPECT_TRUE(v.is_integer32());
        ASSERT_THAT(v.get_integer32().value.size(), 2);
        EXPECT_THAT(v.get_integer32().value[0], -123456789);
        EXPECT_THAT(v.get_integer32().value[1], 66);

        v = makeValue(alloc_, static_cast<std::int16_t>(-1234));
        EXPECT_TRUE(v.is_integer16());
        ASSERT_THAT(v.get_integer16().value.size(), 1);
        EXPECT_THAT(v.get_integer16().value[0], -1234);

        v = makeValue(alloc_, static_cast<std::int8_t>(-128), static_cast<std::int8_t>(+127));
        EXPECT_TRUE(v.is_integer8());
        ASSERT_THAT(v.get_integer8().value.size(), 2);
        EXPECT_THAT(v.get_integer8().value[0], -128);
        EXPECT_THAT(v.get_integer8().value[1], +127);

        v = makeValue(alloc_, static_cast<std::uint64_t>(1234567890), 123, 1234567890123);
        EXPECT_TRUE(v.is_natural64());
        ASSERT_THAT(v.get_natural64().value.size(), 3);
        EXPECT_THAT(v.get_natural64().value[0], 1234567890);
        EXPECT_THAT(v.get_natural64().value[1], 123);
        EXPECT_THAT(v.get_natural64().value[2], 1234567890123);

        v = makeValue(alloc_, static_cast<std::uint32_t>(123456789), static_cast<std::uint32_t>(66));
        EXPECT_TRUE(v.is_natural32());
        ASSERT_THAT(v.get_natural32().value.size(), 2);
        EXPECT_THAT(v.get_natural32().value[0], 123456789);
        EXPECT_THAT(v.get_natural32().value[1], 66);

        v = makeValue(alloc_, static_cast<std::uint16_t>(1234));
        EXPECT_TRUE(v.is_natural16());
        ASSERT_THAT(v.get_natural16().value.size(), 1);
        EXPECT_THAT(v.get_natural16().value[0], 1234);

        v = makeValue(alloc_, static_cast<std::uint8_t>(128), static_cast<std::uint8_t>(127));
        EXPECT_TRUE(v.is_natural8());
        ASSERT_THAT(v.get_natural8().value.size(), 2);
        EXPECT_THAT(v.get_natural8().value[0], 128);
        EXPECT_THAT(v.get_natural8().value[1], 127);
    }

    // Float
    {
        Value v = makeValue(alloc_, +123.456F, -789.523F);
        EXPECT_TRUE(v.is_real32());
        ASSERT_THAT(v.get_real32().value.size(), 2);
        EXPECT_THAT(v.get_real32().value[0], +123.456F);
        EXPECT_THAT(v.get_real32().value[1], -789.523F);

        v = makeValue(alloc_, +123.456F, -789.523F, -1.0);  // One of them is double so all converted to double.
        EXPECT_TRUE(v.is_real64());
        ASSERT_THAT(v.get_real64().value.size(), 3);
        EXPECT_THAT(v.get_real64().value[0], DoubleNear(+123.456, 0.1));
        EXPECT_THAT(v.get_real64().value[1], DoubleNear(-789.523, 0.1));
        EXPECT_THAT(v.get_real64().value[2], DoubleNear(-1.00000, 0.1));
    }

    // Variable size string
    {
        Value v = makeValue(alloc_, "Is it Atreides custom to insult their guests?");
        EXPECT_TRUE(v.is_string());
        EXPECT_THAT(v.get_string().value.size(), 45);
        EXPECT_THAT(reinterpret_cast<const char*>(v.get_string().value.data()),  // NOLINT
                    StartsWith("Is it Atreides custom to insult their guests?"));

        constexpr std::array<cetl::byte, 4> stuff{{b(0x11), b(0x22), b(0x33), b(0x44)}};
        v = makeValue(alloc_, {stuff.data(), stuff.size()});
        EXPECT_TRUE(v.is_unstructured());
        ASSERT_THAT(v.get_unstructured().value.size(), 4);
        EXPECT_THAT(v.get_unstructured().value, ElementsAre(0x11, 0x22, 0x33, 0x44));
    }
}

TEST_F(TestRegistryValue, isVariableSize)
{
    using detail::isVariableSize;

    Value v{alloc_};

    v.set_empty();
    EXPECT_FALSE(isVariableSize(v));

    v.set_string();
    EXPECT_TRUE(isVariableSize(v));
    v.set_unstructured();
    EXPECT_TRUE(isVariableSize(v));

    v.set_bit();
    EXPECT_FALSE(isVariableSize(v));

    v.set_integer8();
    EXPECT_FALSE(isVariableSize(v));
    v.set_integer16();
    EXPECT_FALSE(isVariableSize(v));
    v.set_integer32();
    EXPECT_FALSE(isVariableSize(v));
    v.set_integer64();
    EXPECT_FALSE(isVariableSize(v));

    v.set_natural8();
    EXPECT_FALSE(isVariableSize(v));
    v.set_natural16();
    EXPECT_FALSE(isVariableSize(v));
    v.set_natural32();
    EXPECT_FALSE(isVariableSize(v));
    v.set_natural64();
    EXPECT_FALSE(isVariableSize(v));

    v.set_real16();
    EXPECT_FALSE(isVariableSize(v));
    v.set_real32();
    EXPECT_FALSE(isVariableSize(v));
    v.set_real64();
    EXPECT_FALSE(isVariableSize(v));
}

TEST_F(TestRegistryValue, get)
{
    {
        const Value v{alloc_};

        EXPECT_THAT((get<std::array<int, 3>>(v)), cetl::nullopt);
        EXPECT_THAT((get<std::array<bool, 0>>(v)), cetl::nullopt);
        EXPECT_THAT((get<std::array<bool, 500>>(v)), cetl::nullopt);
        EXPECT_THAT((get<std::array<double, 100>>(v)), cetl::nullopt);
    }
    {
        constexpr std::array<float, 4> f{11'111, 22'222, -12'345, 0};

        const Value v = makeValue(alloc_, std::array<std::int64_t, 3>{{11'111, 22'222, -12'345}});

        EXPECT_THAT((get<std::array<float, 2>>(v)), Optional(ElementsAre(f[0], f[1])));
        EXPECT_THAT((get<std::array<float, 4>>(v)), Optional(ElementsAre(f[0], f[1], f[2], f[3])));
    }
    {
        const Value v = makeValue(alloc_, std::array<bool, 3>{{true, false, true}});
        EXPECT_THAT((get<std::array<bool, 4>>(v)), Optional(ElementsAre(true, false, true, false)));
    }
    {
        const Value v = makeValue(alloc_, std::array<float, 3>{{1, 0, 1}});
        EXPECT_THAT((get<std::array<cetl::byte, 0>>(v)), Optional(IsEmpty()));
        EXPECT_THAT((get<std::array<cetl::byte, 4>>(v)), Optional(ElementsAre(b(1), b(0), b(1), b(0))));
    }
    {
        // Unstructured -> uint8 | array<uint8>
        constexpr std::array<cetl::byte, 4> bytes{{b(1), b(0), b(1), b(0)}};
        const Value                         v = makeValue(alloc_, {bytes.data(), bytes.size()});
        EXPECT_THAT((get<std::uint8_t>(v)), testing::Eq(cetl::nullopt));
        EXPECT_THAT((get<std::array<std::uint8_t, 0>>(v)), testing::Eq(cetl::nullopt));
    }
    {
        // String -> uint8 | array<uint8>
        const Value v = makeValue(alloc_, "abc");
        EXPECT_THAT((get<std::uint8_t>(v)), testing::Eq(cetl::nullopt));
        EXPECT_THAT((get<std::array<std::uint8_t, 0>>(v)), testing::Eq(cetl::nullopt));
    }

    EXPECT_THAT(get<std::int16_t>(makeValue(alloc_, static_cast<std::int64_t>(1234), -9876, 1521)), Optional(1234));
    EXPECT_THAT(get<bool>(makeValue(alloc_, true, false)), Optional(true));
    EXPECT_THAT(get<bool>(makeValue(alloc_, false, true)), Optional(false));
}

TEST_F(TestRegistryValue, coerce)
{
    Value      co_result{alloc_};
    const auto co = [&co_result](const Value& dst, const Value& src) -> const Value* {
        //
        co_result = dst;
        return coerce(co_result, src) ? &co_result : nullptr;
    };
    {
        const auto* v = co(makeValue(alloc_, static_cast<std::int64_t>(0), 0, 0),
                           makeValue(alloc_, static_cast<std::int64_t>(123), 456, 789));
        ASSERT_THAT(v, testing::NotNull());
        EXPECT_TRUE(v->is_integer64());
        ASSERT_THAT(v->get_integer64().value.size(), 3);
        EXPECT_THAT(v->get_integer64().value[0], 123);
        EXPECT_THAT(v->get_integer64().value[1], 456);
        EXPECT_THAT(v->get_integer64().value[2], 789);
    }

    // Empty_1_0 -> Empty_1_0
    // String_1_0 -> String_1_0
    // Unstructured_1_0 -> Unstructured_1_0
    {
        const auto* v = co(Value{alloc_}, Value{alloc_});
        ASSERT_THAT(v, testing::NotNull());
        EXPECT_TRUE(v->is_empty());

        v = co(makeValue(alloc_, "abc"), makeValue(alloc_, "def"));
        ASSERT_THAT(v, testing::NotNull());
        EXPECT_TRUE(v->is_string());
        EXPECT_THAT(v->get_string().value, ElementsAre('d', 'e', 'f'));

        constexpr std::array<cetl::byte, 3> bytes{b(0x11), b(0x22), b(0x33)};
        v = co(makeValue(alloc_, {bytes.data(), 2}), makeValue(alloc_, {bytes.data(), 3}));
        ASSERT_THAT(v, testing::NotNull());
        EXPECT_TRUE(v->is_unstructured());
        EXPECT_THAT(v->get_unstructured().value, ElementsAre(0x11, 0x22, 0x33));
    }
    // String_1_0 -> Unstructured_1_0
    {
        Value                               dstUnstructured{alloc_};
        constexpr std::array<cetl::byte, 2> bytes{b(0x11), b(0x22)};
        set(dstUnstructured, {bytes.data(), bytes.size()});

        const auto* v = co(dstUnstructured, makeValue(alloc_, "def"));
        ASSERT_THAT(v, testing::NotNull());
        EXPECT_TRUE(v->is_unstructured());
        EXPECT_THAT(v->get_unstructured().value, ElementsAre('d', 'e', 'f'));
    }
}

TEST_F(TestRegistryValue, permutate)
{
    Value      co_result{alloc_};
    const auto co = [&co_result](const Value& dst, const Value& src) -> const Value* {
        //
        co_result = dst;
        return coerce(co_result, src) ? &co_result : nullptr;
    };

    Value                               unstructured{alloc_};
    constexpr std::array<cetl::byte, 3> bytes{b(0x11), b(0x22), b(0x33)};
    set(unstructured, {bytes.data(), bytes.size()});
    //
    Value float16{alloc_};
    float16.set_real16();
    float16.get_real16().value.push_back(15.0F);
    //
    const std::array<Value, 15> values = {
        Value{alloc_},
        makeValue(alloc_, "abc"),
        unstructured,
        makeValue(alloc_, true),
        makeValue(alloc_, static_cast<std::int64_t>(4)),
        makeValue(alloc_, static_cast<std::int32_t>(5)),
        makeValue(alloc_, static_cast<std::int16_t>(6)),
        makeValue(alloc_, static_cast<std::int8_t>(7)),
        makeValue(alloc_, static_cast<std::uint64_t>(8)),
        makeValue(alloc_, static_cast<std::uint32_t>(9)),
        makeValue(alloc_, static_cast<std::uint16_t>(10)),
        makeValue(alloc_, static_cast<std::uint8_t>(11)),
        makeValue(alloc_, 12.0),
        makeValue(alloc_, 13.0F),
        float16,
    };
    for (const auto& vi : values)
    {
        for (const auto& vj : values)
        {
            const auto* v = co(vi, vj);
            if (v != nullptr)
            {
                (void) get<bool>(*v);
                (void) get<std::int64_t>(*v);
                (void) get<std::int32_t>(*v);
                (void) get<std::int16_t>(*v);
                (void) get<std::int8_t>(*v);
                (void) get<std::uint64_t>(*v);
                (void) get<std::uint32_t>(*v);
                (void) get<std::uint16_t>(*v);
                (void) get<std::uint8_t>(*v);
                (void) get<float>(*v);
                (void) get<double>(*v);
            }
        }
    }
}

TEST_F(TestRegistryValue, makeName)
{
    auto n = makeName(alloc_, "abc");
    EXPECT_THAT(n.name, ElementsAre('a', 'b', 'c'));

    const std::string too_long_name(256, 'x');
    n = makeName(alloc_, too_long_name);
    EXPECT_THAT(n.name, SizeIs(255));

    n = makeName(alloc_, {nullptr, 0});
    EXPECT_THAT(n.name, IsEmpty());
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
