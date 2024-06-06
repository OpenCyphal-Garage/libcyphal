/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../memory_resource_mock.hpp"
#include "../verification_utilities.hpp"
#include "../tracking_memory_resource.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>
#include <libcyphal/transport/contiguous_payload.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace
{

using namespace libcyphal::transport;  // NOLINT This our main concern here in the unit tests.

using cetl::byte;
using libcyphal::verification_utilities::b;

using testing::_;
using testing::IsNull;
using testing::IsEmpty;
using testing::NotNull;
using testing::Return;
using testing::StrictMock;
using testing::ElementsAre;

class TestContiguousPayload : public testing::Test
{
protected:
    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    // MARK: Data members:

    // NOLINTBEGIN
    TrackingMemoryResource mr_;
    // NOLINTEND
};

// MARK: Tests:

TEST_F(TestContiguousPayload, ctor_data_size)
{
    // Single fragment
    {
        const std::array<byte, 3>                   data123   = {b(1), b(2), b(3)};
        const std::array<cetl::span<const byte>, 1> fragments = {data123};

        const detail::ContiguousPayload payload{mr_, fragments};

        EXPECT_THAT(payload.size(), 3);
        EXPECT_THAT(payload.data(), NotNull());
        const std::vector<byte> v(payload.data(), payload.data() + payload.size());  // NOLINT
        EXPECT_THAT(v, ElementsAre(b(1), b(2), b(3)));
    }
    EXPECT_THAT(mr_.total_allocated_bytes, 0);
    EXPECT_THAT(mr_.total_deallocated_bytes, 0);

    // Double fragments
    {
        const std::array<byte, 3>                   data123   = {b(1), b(2), b(3)};
        const std::array<byte, 2>                   data45    = {b(4), b(5)};
        const std::array<cetl::span<const byte>, 2> fragments = {data123, data45};

        const detail::ContiguousPayload payload{mr_, fragments};

        EXPECT_THAT(payload.size(), 5);
        EXPECT_THAT(payload.data(), NotNull());
        const std::vector<byte> v(payload.data(), payload.data() + payload.size());  // NOLINT
        EXPECT_THAT(v, ElementsAre(b(1), b(2), b(3), b(4), b(5)));
    }
    EXPECT_THAT(mr_.total_allocated_bytes, 5);
    EXPECT_THAT(mr_.total_deallocated_bytes, 5);
}

TEST_F(TestContiguousPayload, ctor_empty_cases)
{
    // No fragments
    {
        const std::array<cetl::span<const byte>, 0> fragments = {};

        const detail::ContiguousPayload payload{mr_, fragments};

        EXPECT_THAT(payload.size(), 0);
        EXPECT_THAT(payload.data(), IsNull());
    }

    // There are fragments, but they are empty
    {
        const std::array<byte, 0>                   data_empty0 = {};
        const std::array<byte, 0>                   data_empty1 = {};
        const std::array<cetl::span<const byte>, 2> fragments   = {data_empty0, data_empty1};

        const detail::ContiguousPayload payload{mr_, fragments};

        EXPECT_THAT(payload.size(), 0);
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
    const std::array<cetl::span<const byte>, 4> fragments   = {data_empty0,
                                                               data123,
                                                               {static_cast<const byte*>(nullptr), 0},
                                                               data_empty3};

    const detail::ContiguousPayload payload{mr_mock, fragments};

    EXPECT_THAT(payload.size(), 3);
    EXPECT_THAT(payload.data(), data123.data());
    const std::vector<byte> v(payload.data(), payload.data() + payload.size());  // NOLINT
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

    const detail::ContiguousPayload payload{mr_mock, fragments};

    EXPECT_THAT(payload.size(), 5);
    EXPECT_THAT(payload.data(), IsNull());
}

}  // namespace
