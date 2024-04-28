/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <libcyphal/transport/can/transport.hpp>

#include "media_mock.hpp"
#include "../multiplexer_mock.hpp"
#include "../../gtest_helpers.hpp"
#include "../../test_utilities.hpp"
#include "../../memory_resource_mock.hpp"
#include "../../virtual_time_scheduler.hpp"
#include "../../tracking_memory_resource.hpp"

#include <gmock/gmock.h>

namespace
{
using byte = cetl::byte;

using namespace libcyphal;
using namespace libcyphal::transport;
using namespace libcyphal::transport::can;
using namespace libcyphal::test_utilities;

using testing::_;
using testing::Eq;
using testing::Return;
using testing::IsNull;
using testing::IsEmpty;
using testing::NotNull;
using testing::Optional;
using testing::StrictMock;
using testing::ElementsAre;
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

    // MARK: Data members:

    VirtualTimeScheduler        scheduler_{};
    TrackingMemoryResource      mr_;
    StrictMock<MediaMock>       media_mock_{};
    StrictMock<MultiplexerMock> mux_mock_{};
};

// MARK: Tests:

TEST_F(TestCanMsgRxSession, make_setTransferIdTimeout)
{
    auto transport = makeTransport(mr_);

    auto maybe_session = transport->makeMessageRxSession({42, 123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));

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

TEST_F(TestCanMsgRxSession, make_fails_due_to_argument_error)
{
    auto transport = makeTransport(mr_);

    // Try invalid subject id
    auto maybe_session = transport->makeMessageRxSession({64, CANARD_SUBJECT_ID_MAX + 1});
    EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<ArgumentError>(_)));
}

TEST_F(TestCanMsgRxSession, run_and_receive)
{
    auto transport = makeTransport(mr_);

    auto maybe_session = transport->makeMessageRxSession({4, 0x23});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));

    {
        SCOPED_TRACE("1-st iteration: one frame available @ 1s");

        scheduler_.setNow(TimePoint{1s});
        const auto rx_timestamp = now();

        EXPECT_CALL(media_mock_, pop(_)).WillOnce([&](auto p) {
            EXPECT_THAT(now(), rx_timestamp + 10ms);
            EXPECT_THAT(p.size(), CANARD_MTU_MAX);
            p[0] = b('0');
            p[1] = b('1');
            p[2] = b(0b111'01101);
            return RxMetadata{rx_timestamp, 0x0C'60'23'45, 3};
        });

        scheduler_.runNow(+10ms, [&] { transport->run(now()); });
        scheduler_.runNow(+10ms, [&] { session->run(now()); });

        const auto maybe_rx_transfer = session->receive();
        ASSERT_THAT(maybe_rx_transfer, Optional(_));
        const auto& rx_transfer = maybe_rx_transfer.value();

        EXPECT_THAT(rx_transfer.metadata.timestamp, rx_timestamp);
        EXPECT_THAT(rx_transfer.metadata.transfer_id, 0x0D);
        EXPECT_THAT(rx_transfer.metadata.priority, Priority::High);
        EXPECT_THAT(rx_transfer.metadata.publisher_node_id, Optional(0x45));

        std::array<char, 2> buffer{};
        ASSERT_THAT(rx_transfer.payload.size(), buffer.size());
        EXPECT_THAT(rx_transfer.payload.copy(0, buffer.data(), buffer.size()), buffer.size());
        EXPECT_THAT(buffer, ElementsAre('0', '1'));
    }
    {
        SCOPED_TRACE("2-nd iteration: no frames available @ 2s");

        scheduler_.setNow(TimePoint{2s});
        const auto rx_timestamp = now();

        EXPECT_CALL(media_mock_, pop(_)).WillOnce([&](auto p) {
            EXPECT_THAT(now(), rx_timestamp + 10ms);
            EXPECT_THAT(p.size(), CANARD_MTU_MAX);
            return cetl::nullopt;
        });

        scheduler_.runNow(+10ms, [&] { transport->run(now()); });
        scheduler_.runNow(+10ms, [&] { session->run(now()); });

        const auto maybe_rx_transfer = session->receive();
        EXPECT_THAT(maybe_rx_transfer, Eq(cetl::nullopt));
    }
    {
        SCOPED_TRACE("3-rd iteration: one anonymous frame available @ 3s");

        scheduler_.setNow(TimePoint{3s});
        const auto rx_timestamp = now();

        EXPECT_CALL(media_mock_, pop(_)).WillOnce([&](auto p) {
            EXPECT_THAT(now(), rx_timestamp + 10ms);
            EXPECT_THAT(p.size(), CANARD_MTU_MAX);
            p[0] = b('1');
            p[1] = b('2');
            p[2] = b(0b111'01110);
            return RxMetadata{rx_timestamp, 0x01'60'23'13, 3};
        });

        scheduler_.runNow(+10ms, [&] { transport->run(now()); });
        scheduler_.runNow(+10ms, [&] { session->run(now()); });

        const auto maybe_rx_transfer = session->receive();
        ASSERT_THAT(maybe_rx_transfer, Optional(_));
        const auto& rx_transfer = maybe_rx_transfer.value();

        EXPECT_THAT(rx_transfer.metadata.timestamp, rx_timestamp);
        EXPECT_THAT(rx_transfer.metadata.transfer_id, 0x0E);
        EXPECT_THAT(rx_transfer.metadata.priority, Priority::Exceptional);
        EXPECT_THAT(rx_transfer.metadata.publisher_node_id, Eq(cetl::nullopt));

        std::array<char, 2> buffer{};
        ASSERT_THAT(rx_transfer.payload.size(), buffer.size());
        EXPECT_THAT(rx_transfer.payload.copy(0, buffer.data(), buffer.size()), buffer.size());
        EXPECT_THAT(buffer, ElementsAre('1', '2'));
    }
}

}  // namespace
