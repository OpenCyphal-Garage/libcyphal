/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <libcyphal/transport/can/transport.hpp>

#include "media_mock.hpp"
#include "../multiplexer_mock.hpp"
#include "../../gtest_helpers.hpp"
#include "../../test_scheduler.hpp"
#include "../../memory_resource_mock.hpp"
#include "../../tracking_memory_resource.hpp"

#include <gmock/gmock.h>

namespace
{
using byte = cetl::byte;

using namespace libcyphal;
using namespace libcyphal::transport;
using namespace libcyphal::transport::can;

using testing::_;
using testing::Eq;
using testing::Return;
using testing::IsNull;
using testing::IsEmpty;
using testing::NotNull;
using testing::Optional;
using testing::StrictMock;
using testing::VariantWith;

using namespace std::chrono_literals;

class TestCanMsgRxSession : public testing::Test
{
protected:
    void SetUp() override
    {
        EXPECT_CALL(media_mock_, getMtu()).WillRepeatedly(Return(CANARD_MTU_MAX));
    }

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        // TODO: Uncomment this when PMR deleter is fixed.
        // EXPECT_EQ(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    TimePoint now() const
    {
        return scheduler_.now();
    }

    CETL_NODISCARD UniquePtr<ICanTransport> makeTransport(cetl::pmr::memory_resource& mr)
    {
        std::array<IMedia*, 1> media_array{&media_mock_};

        auto maybe_transport = can::makeTransport(mr, mux_mock_, media_array, 0, {});
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));
        return cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
    }

    static constexpr byte b(std::uint8_t b)
    {
        return static_cast<byte>(b);
    }

    // MARK: Data members:

    TestScheduler               scheduler_{};
    TrackingMemoryResource      mr_;
    StrictMock<MediaMock>       media_mock_{};
    StrictMock<MultiplexerMock> mux_mock_{};
};

// MARK: Tests:

TEST_F(TestCanMsgRxSession, make_setTransferIdTimeout)
{
    auto transport = makeTransport(mr_);

    auto maybe_session = transport->makeMessageRxSession({42, 123});
    EXPECT_THAT(maybe_session, VariantWith<UniquePtr<IMessageRxSession>>(_));
    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));
    EXPECT_THAT(session, NotNull());

    EXPECT_THAT(session->getParams().extent_bytes, 42);
    EXPECT_THAT(session->getParams().subject_id, 123);

    session->setTransferIdTimeout(0s);
    session->setTransferIdTimeout(500ms);
}

TEST_F(TestCanMsgRxSession, make_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    // Emulate that there is no memory available for the message session.
    EXPECT_CALL(mr_mock, do_allocate(sizeof(can::detail::MessageRxSession), _)).WillOnce(Return(nullptr));

    auto transport = makeTransport(mr_mock);

    auto maybe_session = transport->makeMessageRxSession({64, 0x23});
    EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<MemoryError>(_)));
}

TEST_F(TestCanMsgRxSession, run_and_receive)
{
    auto transport = makeTransport(mr_);

    auto maybe_session = transport->makeMessageRxSession({4, 0x23});
    EXPECT_THAT(maybe_session, VariantWith<UniquePtr<IMessageRxSession>>(_));
    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));
    EXPECT_THAT(session, NotNull());

    // 1-st iteration: one frame available @ 1s
    {
        scheduler_.setNow(TimePoint{1s});
        const auto rx_timestamp = now();

        EXPECT_CALL(media_mock_, pop(_)).WillOnce([&](const cetl::span<byte> payload) {
            EXPECT_THAT(payload.size(), CANARD_MTU_MAX);

            payload[0] = b(42);
            payload[1] = b(147);
            payload[2] = b(0xED);
            return RxMetadata{rx_timestamp, 0x0C'00'23'45, 3};
        });

        scheduler_.runNow(+10ms, [&] { transport->run(now()); });
        scheduler_.runNow(+10ms, [&] { session->run(now()); });

        const auto maybe_rx_transfer = session->receive();
        EXPECT_THAT(maybe_rx_transfer, Optional(_));
        const auto& rx_transfer = maybe_rx_transfer.value();

        EXPECT_THAT(rx_transfer.metadata.timestamp, rx_timestamp);
        EXPECT_THAT(rx_transfer.metadata.transfer_id, 0x0D);
        EXPECT_THAT(rx_transfer.metadata.priority, Priority::High);
        EXPECT_THAT(rx_transfer.metadata.publisher_node_id, Optional(0x45));

        std::array<std::uint8_t, 2> buffer{};
        EXPECT_THAT(rx_transfer.payload.size(), 2);
        EXPECT_THAT(rx_transfer.payload.copy(0, buffer.data(), buffer.size()), 2);
        EXPECT_THAT(buffer[0], 42);
        EXPECT_THAT(buffer[1], 147);
    }

    // 2-nd iteration: no frames available
    {
        scheduler_.setNow(TimePoint{2s});

        EXPECT_CALL(media_mock_, pop(_)).WillOnce([](const cetl::span<byte> payload) {
            EXPECT_THAT(payload.size(), CANARD_MTU_MAX);
            return cetl::nullopt;
        });

        scheduler_.runNow(+10ms, [&] { transport->run(now()); });
        scheduler_.runNow(+10ms, [&] { session->run(now()); });

        const auto maybe_rx_transfer = session->receive();
        EXPECT_THAT(maybe_rx_transfer, Eq(cetl::nullopt));
    }
}

}  // namespace
