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
using testing::ElementsAre;
using testing::VariantWith;
using testing::InSequence;

using namespace std::chrono_literals;

class TestCanMsgTxSession : public testing::Test
{
protected:
    void SetUp() override
    {
        EXPECT_CALL(media_mock_, getMtu()).WillRepeatedly(Return(CANARD_MTU_CAN_CLASSIC));
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

    CETL_NODISCARD UniquePtr<ICanTransport> makeTransport(cetl::pmr::memory_resource& mr,
                                                          const std::size_t           tx_capacity = 16)
    {
        std::array<IMedia*, 1> media_array{&media_mock_};

        auto maybe_transport = can::makeTransport(mr, mux_mock_, media_array, tx_capacity, {});
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));
        return cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
    }

    static constexpr byte b(std::uint8_t b)
    {
        return static_cast<byte>(b);
    }

    template <std::size_t N>
    std::array<byte, N> makeIotaArray(const std::uint8_t init = 0)
    {
        std::array<byte, N> arr{};
        std::iota(reinterpret_cast<std::uint8_t*>(arr.begin()), reinterpret_cast<std::uint8_t*>(arr.end()), init);
        return arr;
    }

    template <std::size_t N>
    std::array<cetl::span<const byte>, 1> makeSpansFrom(const std::array<byte, N>& payload)
    {
        return {payload};
    }

    // MARK: Data members:

    TestScheduler               scheduler_{};
    TrackingMemoryResource      mr_;
    StrictMock<MediaMock>       media_mock_{};
    StrictMock<MultiplexerMock> mux_mock_{};
};

// MARK: Tests:

TEST_F(TestCanMsgTxSession, make)
{
    auto transport = makeTransport(mr_);

    auto maybe_session = transport->makeMessageTxSession({123});
    EXPECT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(_));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));
    EXPECT_THAT(session, NotNull());

    EXPECT_THAT(session->getParams().subject_id, 123);
}

TEST_F(TestCanMsgTxSession, make_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    // Emulate that there is no memory available for the message session.
    EXPECT_CALL(mr_mock, do_allocate(sizeof(can::detail::MessageTxSession), _)).WillOnce(Return(nullptr));

    auto transport = makeTransport(mr_mock);

    auto maybe_session = transport->makeMessageTxSession({0x23});
    EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<MemoryError>(_)));
}

TEST_F(TestCanMsgTxSession, send_empty_payload_and_no_transport_run)
{
    auto transport = makeTransport(mr_);

    auto maybe_session = transport->makeMessageTxSession({123});
    EXPECT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(_));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));
    EXPECT_THAT(session, NotNull());

    const PayloadFragments empty_payload{};
    const TransferMetadata metadata{0x1AF52, {}, Priority::Low};

    auto maybe_error = session->send(metadata, empty_payload);
    EXPECT_THAT(maybe_error, Eq(cetl::nullopt));

    scheduler_.runNow(+10ms, [&] { session->run(scheduler_.now()); });

    // Payload still inside canard TX queue (b/c there was no `transport->run` call deliberately),
    // but there will be no memory leak b/c we expect that it should be deallocated when the transport is destroyed.
    // See `EXPECT_THAT(mr_.allocations, IsEmpty());` at the `TearDown` method.
}

TEST_F(TestCanMsgTxSession, send_empty_payload)
{
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));

    auto transport = makeTransport(mr_);

    auto maybe_session = transport->makeMessageTxSession({123});
    EXPECT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(_));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));
    EXPECT_THAT(session, NotNull());

    scheduler_.runNow(+10s);
    const auto send_time = now();
    const auto timeout   = 1s;

    const PayloadFragments empty_payload{};
    const TransferMetadata metadata{0x3AF52, send_time, Priority::Low};

    auto maybe_error = session->send(metadata, empty_payload);
    EXPECT_THAT(maybe_error, Eq(cetl::nullopt));

    EXPECT_CALL(media_mock_, push(_, _, _))
        .WillOnce([&](const TimePoint deadline, const CanId can_id, const cetl::span<const cetl::byte> payload) {
            EXPECT_THAT(now(), send_time + 10ms);
            EXPECT_THAT(deadline, send_time + timeout);
            EXPECT_THAT(can_id, SubjectOfCanIdEq(123));
            EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsMessageCanId()));

            auto flb = FrameLastByteEq(metadata.transfer_id);
            EXPECT_THAT(payload, ElementsAre(flb));
            return true;
        });

    scheduler_.runNow(+10ms, [&] { transport->run(now()); });
    scheduler_.runNow(+10ms, [&] { transport->run(now()); });
}

TEST_F(TestCanMsgTxSession, send_empty_expired_payload)
{
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));

    auto transport = makeTransport(mr_);

    auto maybe_session = transport->makeMessageTxSession({123});
    EXPECT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(_));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));
    EXPECT_THAT(session, NotNull());

    scheduler_.runNow(+10s);
    const auto send_time = now();
    const auto timeout   = 1s;

    const PayloadFragments empty_payload{};
    const TransferMetadata metadata{0x11, send_time, Priority::Low};

    auto maybe_error = session->send(metadata, empty_payload);
    EXPECT_THAT(maybe_error, Eq(cetl::nullopt));

    // Emulate run calls just on the very edge of the default 1s timeout (exactly at the deadline).
    // Payload should NOT be sent but dropped instead.
    //
    scheduler_.runNow(+timeout, [&] { transport->run(now()); });
    scheduler_.runNow(+1us, [&] { transport->run(now()); });
}

TEST_F(TestCanMsgTxSession, send_7bytes_payload_with_500ms_timeout)
{
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));

    auto transport = makeTransport(mr_);

    auto maybe_session = transport->makeMessageTxSession({17});
    EXPECT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(_));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));
    EXPECT_THAT(session, NotNull());

    const auto timeout = 500ms;
    session->setSendTimeout(timeout);

    scheduler_.runNow(+10s);
    const auto send_time = now();

    const auto             payload = makeIotaArray<CANARD_MTU_CAN_CLASSIC - 1>('0');
    const TransferMetadata metadata{0x03, send_time, Priority::High};

    auto maybe_error = session->send(metadata, makeSpansFrom(payload));
    EXPECT_THAT(maybe_error, Eq(cetl::nullopt));

    // Emulate run calls just on the very edge of the 500ms deadline (just 1us before it).
    // Payload should be sent successfully.
    //
    EXPECT_CALL(media_mock_, push(TimePoint{send_time + timeout}, _, _))
        .WillOnce([&](const TimePoint, const CanId can_id, const cetl::span<const cetl::byte> payload) {
            EXPECT_THAT(now(), send_time + timeout - 1us);
            EXPECT_THAT(can_id, SubjectOfCanIdEq(17));
            EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsMessageCanId()));

            auto flb = FrameLastByteEq(metadata.transfer_id);
            EXPECT_THAT(payload, ElementsAre(b('0'), b('1'), b('2'), b('3'), b('4'), b('5'), b('6'), flb));
            return true;
        });
    //
    scheduler_.runNow(timeout - 1us, [&] { transport->run(now()); });
    scheduler_.runNow(+0us, [&] { transport->run(now()); });
}

TEST_F(TestCanMsgTxSession, sending_multiframe_payload_should_fail_for_anonymous)
{
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));

    auto transport = makeTransport(mr_);

    auto maybe_session = transport->makeMessageTxSession({7});
    EXPECT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(_));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));
    EXPECT_THAT(session, NotNull());

    scheduler_.runNow(+10s);
    const auto send_time = now();

    const auto             payload = makeIotaArray<CANARD_MTU_CAN_CLASSIC>('0');
    const TransferMetadata metadata{0x13, send_time, Priority::Nominal};

    auto maybe_error = session->send(metadata, makeSpansFrom(payload));
    EXPECT_THAT(maybe_error, Optional(VariantWith<ArgumentError>(_)));

    scheduler_.runNow(+10us, [&] { transport->run(now()); });
    scheduler_.runNow(10us, [&] { session->run(now()); });
}

TEST_F(TestCanMsgTxSession, sending_multiframe_payload_for_non_anonymous)
{
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));

    auto transport = makeTransport(mr_);
    EXPECT_THAT(transport->setLocalNodeId(0x45), Eq(cetl::nullopt));

    auto maybe_session = transport->makeMessageTxSession({7});
    EXPECT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(_));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));
    EXPECT_THAT(session, NotNull());

    scheduler_.runNow(+10s);
    const auto timeout   = 1s;
    const auto send_time = now();

    const auto             payload = makeIotaArray<CANARD_MTU_CAN_CLASSIC>('0');
    const TransferMetadata metadata{0x13, send_time, Priority::Nominal};

    auto maybe_error = session->send(metadata, makeSpansFrom(payload));
    EXPECT_THAT(maybe_error, Eq(cetl::nullopt));

    {
        InSequence s;

        EXPECT_CALL(media_mock_, push(_, _, _))
            .WillOnce([&](const TimePoint deadline, const CanId can_id, const cetl::span<const cetl::byte> payload) {
                EXPECT_THAT(now(), send_time + 10us);
                EXPECT_THAT(deadline, send_time + timeout);
                EXPECT_THAT(can_id, SubjectOfCanIdEq(7));
                EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsMessageCanId()));

                auto flb = FrameLastByteEq(metadata.transfer_id, true, false);
                EXPECT_THAT(payload, ElementsAre(b('0'), b('1'), b('2'), b('3'), b('4'), b('5'), b('6'), flb));
                return true;
            });
        EXPECT_CALL(media_mock_, push(_, _, _))
            .WillOnce([&](const TimePoint deadline, const CanId can_id, const cetl::span<const cetl::byte> payload) {
                EXPECT_THAT(now(), send_time + 10us);
                EXPECT_THAT(deadline, send_time + timeout);
                EXPECT_THAT(can_id, SubjectOfCanIdEq(7));
                EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsMessageCanId()));

                auto flb = FrameLastByteEq(metadata.transfer_id, false, true, false);
                EXPECT_THAT(payload, ElementsAre(b('7'), _, _ /* CRC bytes */, flb));
                return true;
            });
    }

    scheduler_.runNow(+10us, [&] { transport->run(now()); });
    scheduler_.runNow(+10us, [&] { transport->run(now()); });
}

}  // namespace
