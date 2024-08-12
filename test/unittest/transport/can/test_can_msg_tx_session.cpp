/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../../cetl_gtest_helpers.hpp"  // NOLINT(misc-include-cleaner)
#include "../../memory_resource_mock.hpp"
#include "../../tracking_memory_resource.hpp"
#include "../../verification_utilities.hpp"
#include "../../virtual_time_scheduler.hpp"
#include "can_gtest_helpers.hpp"
#include "media_mock.hpp"

#include <canard.h>
#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/transport/can/can_transport.hpp>
#include <libcyphal/transport/can/can_transport_impl.hpp>
#include <libcyphal/transport/can/media.hpp>
#include <libcyphal/transport/can/msg_tx_session.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <utility>

namespace
{

using libcyphal::TimePoint;
using libcyphal::UniquePtr;
using namespace libcyphal::transport;       // NOLINT This our main concern here in the unit tests.
using namespace libcyphal::transport::can;  // NOLINT This our main concern here in the unit tests.

using cetl::byte;
using libcyphal::verification_utilities::b;
using libcyphal::verification_utilities::makeIotaArray;
using libcyphal::verification_utilities::makeSpansFrom;

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::Return;
using testing::IsEmpty;
using testing::NotNull;
using testing::Optional;
using testing::StrictMock;
using testing::ElementsAre;
using testing::VariantWith;

// https://github.com/llvm/llvm-project/issues/53444
// NOLINTBEGIN(misc-unused-using-decls, misc-include-cleaner)
using std::literals::chrono_literals::operator""s;
using std::literals::chrono_literals::operator""ms;
using std::literals::chrono_literals::operator""us;
// NOLINTEND(misc-unused-using-decls, misc-include-cleaner)

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestCanMsgTxSession : public testing::Test
{
protected:
    void SetUp() override
    {
        EXPECT_CALL(media_mock_, getMtu())  //
            .WillRepeatedly(Return(CANARD_MTU_CAN_CLASSIC));
        EXPECT_CALL(media_mock_, setFilters(IsEmpty()))  //
            .WillOnce(Return(cetl::nullopt));
    }

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    TimePoint now() const
    {
        return scheduler_.now();
    }

    UniquePtr<ICanTransport> makeTransport(cetl::pmr::memory_resource& mr)
    {
        std::array<IMedia*, 1> media_array{&media_mock_};

        auto maybe_transport = can::makeTransport(mr, scheduler_, media_array, 16);
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));
        return cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
    }

    // MARK: Data members:

    // NOLINTBEGIN
    libcyphal::VirtualTimeScheduler scheduler_{};
    TrackingMemoryResource          mr_;
    StrictMock<MediaMock>           media_mock_{};
    // NOLINTEND
};

// MARK: - Tests:

TEST_F(TestCanMsgTxSession, make)
{
    auto transport = makeTransport(mr_);

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        auto maybe_session = transport->makeMessageTxSession({123});
        ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
        auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

        EXPECT_THAT(session->getParams().subject_id, 123);
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestCanMsgTxSession, make_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock;
    mr_mock.redirectExpectedCallsTo(mr_);

    auto transport = makeTransport(mr_mock);

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        // Emulate that there is no memory available for the message session.
        EXPECT_CALL(mr_mock, do_allocate(sizeof(can::detail::MessageTxSession), _))  //
            .WillOnce(Return(nullptr));

        auto maybe_session = transport->makeMessageTxSession({0x23});
        EXPECT_THAT(maybe_session, VariantWith<AnyFailure>(VariantWith<MemoryError>(_)));
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestCanMsgTxSession, make_fails_due_to_argument_error)
{
    auto transport = makeTransport(mr_);

    // Try invalid subject id
    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        auto maybe_session = transport->makeMessageTxSession({CANARD_SUBJECT_ID_MAX + 1});
        EXPECT_THAT(maybe_session, VariantWith<AnyFailure>(VariantWith<ArgumentError>(_)));
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestCanMsgTxSession, send_empty_payload)
{
    auto transport = makeTransport(mr_);

    auto maybe_session = transport->makeMessageTxSession({123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    constexpr auto timeout = 1s;

    const PayloadFragments empty_payload{};
    TransferMetadata       metadata{0x1AF52, {}, Priority::Low};

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        // Emulate that media has not accepted the payload.
        //
        EXPECT_CALL(media_mock_, push(_, _, _))  //
            .WillOnce([&](auto deadline, auto can_id, auto payload) {
                EXPECT_THAT(now(), metadata.timestamp);
                EXPECT_THAT(deadline, metadata.timestamp + timeout);
                EXPECT_THAT(can_id, SubjectOfCanIdEq(123));
                EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsMessageCanId()));

                auto tbm = TailByteEq(metadata.transfer_id);
                EXPECT_THAT(payload, ElementsAre(tbm));
                return IMedia::PushResult::Success{false /* is_accepted */};
            });
        EXPECT_CALL(media_mock_, registerPushCallback(_))  //
            .WillOnce(Invoke([](auto) { return libcyphal::IExecutor::Callback::Any{}; }));

        metadata.timestamp = now();
        auto failure       = session->send(metadata, empty_payload);
        EXPECT_THAT(failure, Eq(cetl::nullopt));
    });
    scheduler_.spinFor(10s);

    // Payload still inside canard TX queue (b/c media did not accept the payload),
    // but there will be no memory leak b/c we expect that it should be deallocated when the transport is destroyed.
    // See `EXPECT_THAT(mr_.allocations, IsEmpty());` at the `TearDown` method.
}

TEST_F(TestCanMsgTxSession, send_empty_expired_payload)
{
    auto transport = makeTransport(mr_);

    auto maybe_session = transport->makeMessageTxSession({123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    constexpr auto timeout = 1s;

    const PayloadFragments empty_payload{};
    TransferMetadata       metadata{0x11, {}, Priority::Low};

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        // Emulate that media became ready on the very edge of the default 1s timeout (exactly at the deadline).
        EXPECT_CALL(media_mock_, push(_, _, _))  //
            .WillOnce(Return(IMedia::PushResult::Success{false /* is_accepted */}));
        EXPECT_CALL(media_mock_, registerPushCallback(_))  //
            .WillOnce(Invoke([&](auto function) {          //
                return scheduler_.registerAndScheduleNamedCallback("", now() + timeout, std::move(function));
            }));

        metadata.timestamp = now();
        auto failure       = session->send(metadata, empty_payload);
        EXPECT_THAT(failure, Eq(cetl::nullopt));
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestCanMsgTxSession, send_7bytes_payload_with_500ms_timeout)
{
    auto transport = makeTransport(mr_);

    auto maybe_session = transport->makeMessageTxSession({17});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    constexpr auto timeout = 500ms;
    session->setSendTimeout(timeout);

    const auto       payload = makeIotaArray<CANARD_MTU_CAN_CLASSIC - 1>(b('1'));
    TransferMetadata metadata{0x03, {}, Priority::High};

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        // Emulate that socket became ready on the very edge of the 500ms timeout (just 1us before the deadline).
        EXPECT_CALL(media_mock_, push(_, _, _))  //
            .WillOnce(Return(IMedia::PushResult::Success{false /* is_accepted */}));
        EXPECT_CALL(media_mock_, registerPushCallback(_))  //
            .WillOnce(Invoke([&](auto function) {          //
                return scheduler_.registerAndScheduleNamedCallback("", now() + timeout - 1us, std::move(function));
            }));

        metadata.timestamp = now();
        auto failure       = session->send(metadata, makeSpansFrom(payload));
        EXPECT_THAT(failure, Eq(cetl::nullopt));
    });
    scheduler_.scheduleAt(1s + timeout - 1us, [&](const auto&) {
        //
        EXPECT_CALL(media_mock_, push(_, _, _))  //
            .WillOnce([&](auto, auto can_id, auto payload) {
                EXPECT_THAT(now(), metadata.timestamp + timeout - 1us);
                EXPECT_THAT(can_id, SubjectOfCanIdEq(17));
                EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsMessageCanId()));

                auto tbm = TailByteEq(metadata.transfer_id);
                EXPECT_THAT(payload, ElementsAre(b('1'), b('2'), b('3'), b('4'), b('5'), b('6'), b('7'), tbm));
                return IMedia::PushResult::Success{true /* is_accepted */};
            });
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestCanMsgTxSession, send_when_no_memory_for_contiguous_payload)
{
    StrictMock<MemoryResourceMock> mr_mock;
    mr_mock.redirectExpectedCallsTo(mr_);

    auto transport = makeTransport(mr_mock);

    // Emulate that there is no memory available for the expected contiguous payload.
    const auto payload1 = makeIotaArray<1>(b('0'));
    const auto payload2 = makeIotaArray<2>(b('1'));
    EXPECT_CALL(mr_mock, do_allocate(sizeof(payload1) + sizeof(payload2), _))  //
        .WillOnce(Return(nullptr));

    auto maybe_session = transport->makeMessageTxSession({17});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    TransferMetadata metadata{0x03, {}, Priority::Optional};

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        metadata.timestamp = now();
        auto failure       = session->send(metadata, makeSpansFrom(payload1, payload2));
        EXPECT_THAT(failure, Optional(VariantWith<MemoryError>(_)));
    });
    scheduler_.spinFor(10s);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
