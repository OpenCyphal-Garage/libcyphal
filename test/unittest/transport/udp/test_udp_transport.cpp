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

    TrackingMemoryResource      mr_;
    StrictMock<MediaMock>       media_mock_{};
    StrictMock<MultiplexerMock> mux_mock_{};
};

// MARK: Tests:

TEST_F(TestUpdTransport, makeTransport)
{
    // Anonymous node
    {
        std::array<IMedia*, 1> media_array{&media_mock_};
        auto                   maybe_transport = makeTransport(mr_, mux_mock_, media_array, {});
        EXPECT_THAT(maybe_transport, VariantWith<FactoryError>(VariantWith<NotImplementedError>(_)));
    }
}

}  // namespace
