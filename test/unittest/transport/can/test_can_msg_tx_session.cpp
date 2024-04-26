/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <libcyphal/transport/can/transport.hpp>

#include "media_mock.hpp"
#include "../multiplexer_mock.hpp"
#include "../../gtest_helpers.hpp"
#include "../../test_scheduler.hpp"
#include "../../test_utilities.hpp"
#include "../../memory_resource_mock.hpp"
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
using testing::InSequence;
using testing::StrictMock;
using testing::ElementsAre;
using testing::VariantWith;

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

    // MARK: Data members:

    TestScheduler               scheduler_{};
    TrackingMemoryResource      mr_;
    StrictMock<MultiplexerMock> mux_mock_{};
    StrictMock<MediaMock>       media_mock_{};
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

TEST_F(TestCanMsgTxSession, make_fails_due_to_argument_error)
{
    auto transport = makeTransport(mr_);

    // Try invalid subject id
    auto maybe_session = transport->makeMessageTxSession({CANARD_SUBJECT_ID_MAX + 1});
    EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<ArgumentError>(_)));
}

TEST_F(TestCanMsgTxSession, send_empty_payload_and_no_transport_run)
{
    auto transport = makeTransport(mr_);

    auto maybe_session = transport->makeMessageTxSession({123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(_));
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
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(_));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));
    EXPECT_THAT(session, NotNull());

    scheduler_.runNow(+10s);
    const auto send_time = now();
    const auto timeout   = 1s;

    const PayloadFragments empty_payload{};
    const TransferMetadata metadata{0x3AF52, send_time, Priority::Low};

    auto maybe_error = session->send(metadata, empty_payload);
    EXPECT_THAT(maybe_error, Eq(cetl::nullopt));

    EXPECT_CALL(media_mock_, push(_, _, _)).WillOnce([&](auto deadline, auto can_id, auto payload) {
        EXPECT_THAT(now(), send_time + 10ms);
        EXPECT_THAT(deadline, send_time + timeout);
        EXPECT_THAT(can_id, SubjectOfCanIdEq(123));
        EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsMessageCanId()));

        auto tbm = TailByteEq(metadata.transfer_id);
        EXPECT_THAT(payload, ElementsAre(tbm));
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
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(_));
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
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(_));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));
    EXPECT_THAT(session, NotNull());

    const auto timeout = 500ms;
    session->setSendTimeout(timeout);

    scheduler_.runNow(+10s);
    const auto send_time = now();

    const auto             payload = makeIotaArray<CANARD_MTU_CAN_CLASSIC - 1>('1');
    const TransferMetadata metadata{0x03, send_time, Priority::High};

    auto maybe_error = session->send(metadata, makeSpansFrom(payload));
    EXPECT_THAT(maybe_error, Eq(cetl::nullopt));

    // Emulate run calls just on the very edge of the 500ms deadline (just 1us before it).
    // Payload should be sent successfully.
    //
    EXPECT_CALL(media_mock_, push(TimePoint{send_time + timeout}, _, _)).WillOnce([&](auto, auto can_id, auto payload) {
        EXPECT_THAT(now(), send_time + timeout - 1us);
        EXPECT_THAT(can_id, SubjectOfCanIdEq(17));
        EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsMessageCanId()));

        auto tbm = TailByteEq(metadata.transfer_id);
        EXPECT_THAT(payload, ElementsAre(b('1'), b('2'), b('3'), b('4'), b('5'), b('6'), b('7'), tbm));
        return true;
    });
    //
    scheduler_.runNow(timeout - 1us, [&] { transport->run(now()); });
    scheduler_.runNow(+0us, [&] { transport->run(now()); });
}

TEST_F(TestCanMsgTxSession, send_when_no_memory_for_contiguous_payload)
{
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));

    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    auto transport = makeTransport(mr_mock);

    // Emulate that there is no memory available for the expected contiguous payload.
    const auto payload1 = makeIotaArray<1>('0');
    const auto payload2 = makeIotaArray<2>('1');
    EXPECT_CALL(mr_mock, do_allocate(sizeof(payload1) + sizeof(payload2), _)).WillOnce(Return(nullptr));

    auto maybe_session = transport->makeMessageTxSession({17});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(_));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));
    EXPECT_THAT(session, NotNull());

    scheduler_.runNow(+10s);
    const auto send_time = now();

    const TransferMetadata metadata{0x03, send_time, Priority::Optional};

    auto maybe_error = session->send(metadata, makeSpansFrom(payload1, payload2));
    EXPECT_THAT(maybe_error, Optional(VariantWith<MemoryError>(_)));

    scheduler_.runNow(+10ms, [&] { transport->run(scheduler_.now()); });
}

}  // namespace
