/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "tracking_memory_resource.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/media_payload.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <utility>

namespace
{

using namespace libcyphal::transport;  // NOLINT This our main concern here in the unit tests.

using testing::IsEmpty;

class TestMediaPayload : public testing::Test
{
protected:
    void SetUp() override
    {
        cetl::pmr::set_default_resource(&mr_);
    }

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

// MARK: - Tests:

TEST_F(TestMediaPayload, default_ctor)
{
    MediaPayload payload;
    EXPECT_THAT(payload.getSpan().size(), 0);
    EXPECT_THAT(payload.getSpan().data(), nullptr);
    EXPECT_THAT(payload.getAllocatedSize(), 0);

    // It's fine to attempt to reset or release an empty payload.
    const auto fields = payload.release();
    EXPECT_THAT(std::get<0>(fields), 0);
    EXPECT_THAT(std::get<1>(fields), nullptr);
    EXPECT_THAT(std::get<2>(fields), 0);

    payload.reset();
}

TEST_F(TestMediaPayload, main_ctor)
{
    constexpr std::size_t payload_size           = 5;
    constexpr std::size_t payload_allocated_size = 8;
    auto* const           payload_data           = static_cast<cetl::byte*>(mr_.allocate(payload_allocated_size));

    const MediaPayload payload{payload_size, payload_data, payload_allocated_size, &mr_};
    EXPECT_THAT(payload.getSpan().size(), payload_size);
    EXPECT_THAT(payload.getSpan().data(), payload_data);
    EXPECT_THAT(payload.getAllocatedSize(), payload_allocated_size);
}

TEST_F(TestMediaPayload, move_ctor)
{
    constexpr std::size_t payload_size           = 5;
    constexpr std::size_t payload_allocated_size = 8;
    auto* const           payload_data           = static_cast<cetl::byte*>(mr_.allocate(payload_allocated_size));

    MediaPayload payload1{payload_size, payload_data, payload_allocated_size, &mr_};

    const MediaPayload payload2{std::move(payload1)};

    EXPECT_THAT(payload2.getSpan().size(), payload_size);
    EXPECT_THAT(payload2.getSpan().data(), payload_data);
    EXPECT_THAT(payload2.getAllocatedSize(), payload_allocated_size);
}

TEST_F(TestMediaPayload, move_assignment)
{
    constexpr std::size_t payload_size           = 5;
    constexpr std::size_t payload_allocated_size = 8;
    auto* const           payload_data           = static_cast<cetl::byte*>(mr_.allocate(payload_allocated_size));

    MediaPayload payload1{payload_size, payload_data, payload_allocated_size, &mr_};

    MediaPayload payload2{};
    payload2 = std::move(payload1);

    EXPECT_THAT(payload2.getSpan().size(), payload_size);
    EXPECT_THAT(payload2.getSpan().data(), payload_data);
    EXPECT_THAT(payload2.getAllocatedSize(), payload_allocated_size);
}

TEST_F(TestMediaPayload, release)
{
    constexpr std::size_t payload_size           = 5;
    constexpr std::size_t payload_allocated_size = 8;
    auto* const           payload_data           = static_cast<cetl::byte*>(mr_.allocate(payload_allocated_size));

    MediaPayload payload{payload_size, payload_data, payload_allocated_size, &mr_};

    auto fields = payload.release();
    EXPECT_THAT(std::get<0>(fields), payload_size);
    EXPECT_THAT(std::get<1>(fields), payload_data);
    EXPECT_THAT(std::get<2>(fields), payload_allocated_size);
    mr_.deallocate(std::get<1>(fields), std::get<2>(fields));

    fields = payload.release();
    EXPECT_THAT(std::get<0>(fields), 0);
    EXPECT_THAT(std::get<1>(fields), nullptr);
    EXPECT_THAT(std::get<2>(fields), 0);
}

TEST_F(TestMediaPayload, reset)
{
    constexpr std::size_t payload_size           = 5;
    constexpr std::size_t payload_allocated_size = 8;
    auto* const           payload_data           = static_cast<cetl::byte*>(mr_.allocate(payload_allocated_size));

    MediaPayload payload{payload_size, payload_data, payload_allocated_size, &mr_};

    payload.reset();
    EXPECT_THAT(payload.getSpan().size(), 0);
    EXPECT_THAT(payload.getSpan().data(), nullptr);
    EXPECT_THAT(payload.getAllocatedSize(), 0);

    payload.reset();
    EXPECT_THAT(payload.getSpan().size(), 0);
    EXPECT_THAT(payload.getSpan().data(), nullptr);
    EXPECT_THAT(payload.getAllocatedSize(), 0);
}

}  // namespace
