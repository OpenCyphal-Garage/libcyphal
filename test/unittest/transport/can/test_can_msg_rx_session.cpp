/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "media_mock.hpp"
#include "../multiplexer_mock.hpp"
#include "../../tracking_memory_resource.hpp"

#include <cetl/pf17/variant.hpp>
#include <libcyphal/transport/can/transport.hpp>

#include <chrono>
#include <gmock/gmock.h>

namespace
{
using namespace libcyphal;
using namespace libcyphal::transport;
using namespace libcyphal::transport::can;

using testing::_;
using testing::StrictMock;

using namespace std::chrono_literals;

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

TEST_F(TestCanMsgRxSession, run_receive)
{
    auto transport = makeTransport();

    auto maybe_session = transport->makeMessageRxSession({4, 0x23});
    auto session       = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));
    EXPECT_TRUE(session);

    EXPECT_CALL(media_mock_, pop(_)).WillOnce([](const cetl::span<cetl::byte> payload) {
        EXPECT_EQ(CANARD_MTU_MAX, payload.size());

        payload[0] = static_cast<cetl::byte>(42);
        payload[1] = static_cast<cetl::byte>(147);
        payload[2] = static_cast<cetl::byte>(0xED);
        return RxMetadata{TimePoint{10s}, 0x0C002345, 3};
    });

    transport->run(TimePoint{10s + 2ms});

    auto maybe_rx_transfer = session->receive();
    EXPECT_TRUE(maybe_rx_transfer.has_value());
    const auto& rx_transfer = maybe_rx_transfer.value();

    EXPECT_EQ(TimePoint{10s}, rx_transfer.metadata.timestamp);
    EXPECT_EQ(0x0D, rx_transfer.metadata.transfer_id);
    EXPECT_EQ(Priority::High, rx_transfer.metadata.priority);
    EXPECT_EQ(0x45, rx_transfer.metadata.publisher_node_id);

    std::array<std::uint8_t, 2> buffer{};
    EXPECT_EQ(2, rx_transfer.payload.size());
    EXPECT_EQ(2, rx_transfer.payload.copy(0, buffer.data(), buffer.size()));
    EXPECT_EQ(42, buffer[0]);
    EXPECT_EQ(147, buffer[1]);
}

}  // namespace
