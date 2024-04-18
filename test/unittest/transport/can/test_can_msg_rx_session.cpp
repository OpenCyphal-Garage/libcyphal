/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <libcyphal/transport/can/transport.hpp>

#include "media_mock.hpp"
#include "../multiplexer_mock.hpp"
#include "../../gtest_helpers.hpp"
#include "../../tracking_memory_resource.hpp"

#include <gmock/gmock.h>

namespace
{
using namespace libcyphal;
using namespace libcyphal::transport;
using namespace libcyphal::transport::can;

using testing::_;
using testing::Eq;
using testing::IsNull;
using testing::NotNull;
using testing::Optional;
using testing::StrictMock;
using testing::VariantWith;

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
        auto maybe_transport = can::makeTransport(mr_, mux_mock_, {&media_mock_}, {});
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(_));
        return cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
    }

    // MARK: Data members:

    TrackingMemoryResource      mr_;
    StrictMock<MediaMock>       media_mock_{};
    StrictMock<MultiplexerMock> mux_mock_{};
};

// MARK: Tests:

TEST_F(TestCanMsgRxSession, make_setTransferIdTimeout)
{
    auto transport = makeTransport();

    auto maybe_session = transport->makeMessageRxSession({42, 123});
    EXPECT_THAT(maybe_session, VariantWith<UniquePtr<IMessageRxSession>>(_));
    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));
    EXPECT_THAT(session, NotNull());

    EXPECT_THAT(session->getParams().extent_bytes, Eq(42));
    EXPECT_THAT(session->getParams().subject_id, Eq(123));

    session->setTransferIdTimeout(0s);
    session->setTransferIdTimeout(500ms);
}

TEST_F(TestCanMsgRxSession, run_receive)
{
    auto transport = makeTransport();

    auto maybe_session = transport->makeMessageRxSession({4, 0x23});
    EXPECT_THAT(maybe_session, VariantWith<UniquePtr<IMessageRxSession>>(_));
    auto session       = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));
    EXPECT_THAT(session, NotNull());

    // 1-st iteration: one frame available @ 1s
    {
        EXPECT_CALL(media_mock_, pop(_)).WillOnce([](const cetl::span<cetl::byte> payload) {
            EXPECT_THAT(payload.size(), Eq(CANARD_MTU_MAX));

            payload[0] = static_cast<cetl::byte>(42);
            payload[1] = static_cast<cetl::byte>(147);
            payload[2] = static_cast<cetl::byte>(0xED);
            return RxMetadata{TimePoint{1s}, 0x0C002345, 3};
        });

        transport->run(TimePoint{1s + 10ms});

        session->run(TimePoint{1s + 20ms});

        const auto maybe_rx_transfer = session->receive();
        EXPECT_THAT(maybe_rx_transfer, Optional(_));
        const auto& rx_transfer = maybe_rx_transfer.value();

        EXPECT_THAT(rx_transfer.metadata.timestamp, Eq(TimePoint{1s}));
        EXPECT_THAT(rx_transfer.metadata.transfer_id, Eq(0x0D));
        EXPECT_THAT(rx_transfer.metadata.priority, Eq(Priority::High));
        EXPECT_THAT(rx_transfer.metadata.publisher_node_id, Optional(0x45));

        std::array<std::uint8_t, 2> buffer{};
        EXPECT_THAT(rx_transfer.payload.size(), Eq(2));
        EXPECT_THAT(rx_transfer.payload.copy(0, buffer.data(), buffer.size()), Eq(2));
        EXPECT_THAT(buffer[0], Eq(42));
        EXPECT_THAT(buffer[1], Eq(147));
    }

    // 2-nd iteration: no frames available
    {
        EXPECT_CALL(media_mock_, pop(_)).WillOnce([](const cetl::span<cetl::byte> payload) {
            EXPECT_THAT(payload.size(), Eq(CANARD_MTU_MAX));
            return cetl::nullopt;
        });

        transport->run(TimePoint{2s + 10ms});

        session->run(TimePoint{2s + 20ms});

        const auto maybe_rx_transfer = session->receive();
        EXPECT_THAT(maybe_rx_transfer, Eq(cetl::nullopt));
    }
}

}  // namespace
