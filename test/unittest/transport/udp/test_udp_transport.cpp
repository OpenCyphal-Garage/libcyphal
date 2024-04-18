/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <libcyphal/transport/udp/transport.hpp>

#include "media_mock.hpp"
#include "../multiplexer_mock.hpp"
#include "../../tracking_memory_resource.hpp"

#include <gmock/gmock.h>

namespace
{
using namespace libcyphal;
using namespace libcyphal::transport;
using namespace libcyphal::transport::udp;

using testing::_;
using testing::StrictMock;
using testing::VariantWith;

class TestUpdTransport : public testing::Test
{
protected:
    void TearDown() override
    {
        // TODO: Uncomment this when PMR deleter is fixed.
        // EXPECT_EQ(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    // MARK: Data members:

    TrackingMemoryResource      mr_;
    StrictMock<MediaMock>       media_mock_{};
    StrictMock<MultiplexerMock> mux_mock_{};
};

// MARK: Tests:

TEST_F(TestUpdTransport, makeTransport)
{
    // Anonymous node
    {
        auto maybe_transport = makeTransport(mr_, mux_mock_, {&media_mock_}, {});
        EXPECT_THAT(maybe_transport, VariantWith<FactoryError>(VariantWith<NotImplementedError>(_)));
    }
}

}  // namespace
