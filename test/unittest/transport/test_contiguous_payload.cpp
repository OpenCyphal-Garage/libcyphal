/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <libcyphal/transport/contiguous_payload.hpp>

#include "../memory_resource_mock.hpp"
#include "../tracking_memory_resource.hpp"

#include <vector>
#include <gmock/gmock.h>

namespace
{
using byte              = cetl::byte;
using ContiguousPayload = libcyphal::transport::detail::ContiguousPayload;

using testing::_;
using testing::Eq;
using testing::IsNull;
using testing::NotNull;
using testing::Return;
using testing::StrictMock;
using testing::ElementsAre;

class TestContiguousPayload : public testing::Test
{
protected:
    void TearDown() override
    {
        EXPECT_EQ(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    static constexpr byte b(std::uint8_t b)
    {
        return static_cast<byte>(b);
    }

    // MARK: Data members:

    TrackingMemoryResource mr_;
};

// MARK: Tests:

TEST_F(TestContiguousPayload, ctor_data_size)
{
    // Single fragment
    {
        const std::array<byte, 3>                   data123   = {b(1), b(2), b(3)};
        const std::array<cetl::span<const byte>, 1> fragments = {data123};

        const ContiguousPayload payload{mr_, fragments};

        EXPECT_THAT(payload.size(), Eq(3));
        EXPECT_THAT(payload.data(), NotNull());
        const std::vector<byte> v(payload.data(), payload.data() + payload.size());
        EXPECT_THAT(v, ElementsAre(b(1), b(2), b(3)));
    }
    EXPECT_THAT(mr_.total_allocated_bytes, Eq(0));
    EXPECT_THAT(mr_.total_deallocated_bytes, Eq(0));

    // Double fragments
    {
        const std::array<byte, 3>                   data123   = {b(1), b(2), b(3)};
        const std::array<byte, 2>                   data45    = {b(4), b(5)};
        const std::array<cetl::span<const byte>, 2> fragments = {data123, data45};

        const ContiguousPayload payload{mr_, fragments};

        EXPECT_THAT(payload.size(), Eq(5));
        EXPECT_THAT(payload.data(), NotNull());
        const std::vector<byte> v(payload.data(), payload.data() + payload.size());
        EXPECT_THAT(v, ElementsAre(b(1), b(2), b(3), b(4), b(5)));
    }
    EXPECT_THAT(mr_.total_allocated_bytes, Eq(5));
    EXPECT_THAT(mr_.total_deallocated_bytes, Eq(5));
}

TEST_F(TestContiguousPayload, ctor_empty_cases)
{
    // No fragments
    {
        const std::array<cetl::span<const byte>, 0> fragments = {};

        const ContiguousPayload payload{mr_, fragments};

        EXPECT_THAT(payload.size(), Eq(0));
        EXPECT_THAT(payload.data(), IsNull());
    }

    // There are fragments, but they are empty
    {
        const std::array<byte, 0>                   data_empty0 = {};
        const std::array<byte, 0>                   data_empty1 = {};
        const std::array<cetl::span<const byte>, 2> fragments   = {data_empty0, data_empty1};

        const ContiguousPayload payload{mr_, fragments};

        EXPECT_THAT(payload.size(), Eq(0));
        EXPECT_THAT(payload.data(), IsNull());
    }
}

TEST_F(TestContiguousPayload, ctor_no_alloc_for_single_non_empty_fragment)
{
    StrictMock<MemoryResourceMock> mr_mock{};

    // 4 fragments, but only 1 is non-empty
    const std::array<byte, 0>                   data_empty0 = {};
    const std::array<byte, 3>                   data123     = {b(1), b(2), b(3)};
    const std::array<byte, 0>                   data_empty3 = {};
    const std::array<cetl::span<const byte>, 4> fragments   = {data_empty0, data123, {nullptr, 0}, data_empty3};

    const ContiguousPayload payload{mr_mock, fragments};

    EXPECT_THAT(payload.size(), Eq(3));
    EXPECT_THAT(payload.data(), Eq(data123.data()));
    const std::vector<byte> v(payload.data(), payload.data() + payload.size());
    EXPECT_THAT(v, ElementsAre(b(1), b(2), b(3)));
}

TEST_F(TestContiguousPayload, ctor_no_memory_error)
{
    StrictMock<MemoryResourceMock> mr_mock{};

    // Emulate that there is no memory available for the transport.
    EXPECT_CALL(mr_mock, do_allocate(_, _)).WillOnce(Return(nullptr));

    const std::array<byte, 3>                   data123   = {b(1), b(2), b(3)};
    const std::array<byte, 2>                   data45    = {b(4), b(5)};
    const std::array<cetl::span<const byte>, 2> fragments = {data123, data45};

    const ContiguousPayload payload{mr_mock, fragments};

    EXPECT_THAT(payload.size(), Eq(5));
    EXPECT_THAT(payload.data(), IsNull());
}

}  // namespace
