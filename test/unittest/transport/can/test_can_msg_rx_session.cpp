/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "media_mock.hpp"
#include "../multiplexer_mock.hpp"
#include "../../tracking_memory_resource.hpp"

#include <cetl/pf17/variant.hpp>
#include <libcyphal/transport/can/transport.hpp>

#include <gmock/gmock.h>

namespace
{
using namespace libcyphal;
using namespace libcyphal::transport;
using namespace libcyphal::transport::can;

using testing::StrictMock;

class TestCanMsgRxSession : public testing::Test
{
protected:
    void TearDown() override
    {
        // TODO: Uncomment this when PMR deleter is fixed.
        // EXPECT_EQ(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    CETL_NODISCARD UniquePtr<ICanTransport> makeTransport()
    {
        return cetl::get<UniquePtr<ICanTransport>>(can::makeTransport(mr_, mux_mock_, {&media_mock_}, {}));
    }

    TrackingMemoryResource      mr_;
    StrictMock<MediaMock>       media_mock_{};
    StrictMock<MultiplexerMock> mux_mock_{};
};

// MARK: Tests:

TEST_F(TestCanMsgRxSession, make)
{
    auto transport = makeTransport();

    auto maybe_rx_session = transport->makeMessageRxSession({42, 123});
    auto session          = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_rx_session));
    EXPECT_TRUE(session);

    EXPECT_EQ(42, session->getParams().extent_bytes);
    EXPECT_EQ(123, session->getParams().subject_id);
}

}  // namespace
