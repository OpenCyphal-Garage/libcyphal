/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../../tracking_memory_resource.hpp"
#include "../multiplexer_mock.hpp"
#include "media_mock.hpp"

#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/udp/media.hpp>
#include <libcyphal/transport/udp/transport.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>

namespace
{

using namespace libcyphal::transport;  // NOLINT This our main concern here in the unit tests.

using testing::_;
using testing::IsEmpty;
using testing::StrictMock;
using testing::VariantWith;

class TestUpdTransport : public testing::Test
{
protected:
    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        // TODO: Uncomment this when PMR deleter is fixed.
        // EXPECT_EQ(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    // MARK: Data members:

    // NOLINTBEGIN
    TrackingMemoryResource      mr_;
    StrictMock<MultiplexerMock> mux_mock_{};
    StrictMock<udp::MediaMock>  media_mock_{};
    // NOLINTEND
};

// MARK: Tests:

TEST_F(TestUpdTransport, makeTransport)
{
    // Anonymous node
    {
        std::array<udp::IMedia*, 1> media_array{&media_mock_};
        auto                        maybe_transport = makeTransport(mr_, mux_mock_, media_array, {});
        EXPECT_THAT(maybe_transport, VariantWith<FactoryError>(VariantWith<NotImplementedError>(_)));
    }
}

}  // namespace
