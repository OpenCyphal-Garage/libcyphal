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
#include "transient_error_handler_mock.hpp"

#include <canard.h>
#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/can/can_transport.hpp>
#include <libcyphal/transport/can/can_transport_impl.hpp>
#include <libcyphal/transport/can/media.hpp>
#include <libcyphal/transport/can/msg_rx_session.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <functional>
#include <utility>

namespace
{

using libcyphal::TimePoint;
using libcyphal::UniquePtr;
using namespace libcyphal::transport;       // NOLINT This our main concern here in the unit tests.
using namespace libcyphal::transport::can;  // NOLINT This our main concern here in the unit tests.

using cetl::byte;
using libcyphal::verification_utilities::b;

using testing::_;
using testing::Eq;
using testing::Ref;
using testing::Invoke;
using testing::Return;
using testing::SizeIs;
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
// NOLINTEND(misc-unused-using-decls, misc-include-cleaner)

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestCanMsgRxSession : public testing::Test
{
protected:
    void SetUp() override
    {
        EXPECT_CALL(media_mock_, getMtu())  //
            .WillRepeatedly(Return(CANARD_MTU_MAX));
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

        auto maybe_transport = can::makeTransport(mr, scheduler_, media_array, 0);
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

TEST_F(TestCanMsgRxSession, make_setTransferIdTimeout)
{
    auto transport = makeTransport(mr_);

    EXPECT_CALL(media_mock_, registerPopCallback(Ref(scheduler_), _))  //
        .WillOnce(Invoke([&](auto&, auto function) {                   //
            return scheduler_.registerNamedCallback("rx", std::move(function));
        }));

    auto maybe_session = transport->makeMessageRxSession({42, 123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));

    EXPECT_THAT(session->getParams().extent_bytes, 42);
    EXPECT_THAT(session->getParams().subject_id, 123);

    session->setTransferIdTimeout(0s);
    session->setTransferIdTimeout(500ms);

    EXPECT_THAT(scheduler_.hasNamedCallback("rx"), true);
    session.reset();
    EXPECT_THAT(scheduler_.hasNamedCallback("rx"), false);
}

TEST_F(TestCanMsgRxSession, make_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock;
    mr_mock.redirectExpectedCallsTo(mr_);

    auto transport = makeTransport(mr_mock);

    // Emulate that there is no memory available for the message session.
    EXPECT_CALL(mr_mock, do_allocate(sizeof(can::detail::MessageRxSession), _))  //
        .WillOnce(Return(nullptr));

    auto maybe_session = transport->makeMessageRxSession({64, 0x23});
    EXPECT_THAT(maybe_session, VariantWith<AnyFailure>(VariantWith<MemoryError>(_)));
}

TEST_F(TestCanMsgRxSession, make_fails_due_to_argument_error)
{
    auto transport = makeTransport(mr_);

    // Try invalid subject id
    auto maybe_session = transport->makeMessageRxSession({64, CANARD_SUBJECT_ID_MAX + 1});
    EXPECT_THAT(maybe_session, VariantWith<AnyFailure>(VariantWith<ArgumentError>(_)));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_F(TestCanMsgRxSession, receive)
{
    StrictMock<TransientErrorHandlerMock> handler_mock;

    auto transport = makeTransport(mr_);
    transport->setTransientErrorHandler(std::ref(handler_mock));

    EXPECT_CALL(media_mock_, registerPopCallback(Ref(scheduler_), _))  //
        .WillOnce(Invoke([&](auto&, auto function) {                   //
            return scheduler_.registerNamedCallback("rx", std::move(function));
        }));

    auto maybe_session = transport->makeMessageRxSession({4, 0x23});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));

    EXPECT_CALL(media_mock_, setFilters(SizeIs(1)))  //
        .WillOnce([&](Filters filters) {
            EXPECT_THAT(filters, Contains(FilterEq({0x2300, 0x21FFF80})));
            return cetl::nullopt;
        });

    const auto params = session->getParams();
    EXPECT_THAT(params.extent_bytes, 4);
    EXPECT_THAT(params.subject_id, 0x23);

    constexpr auto timeout = 200ms;
    session->setTransferIdTimeout(timeout);

    TimePoint rx_timestamp;

    scheduler_.scheduleAt(1s, [&](const TimePoint) {
        //
        SCOPED_TRACE("1-st iteration: one frame available @ 1s");

        rx_timestamp = now() + 10ms;
        EXPECT_CALL(media_mock_, pop(_))  //
            .WillOnce([&](auto p) {
                EXPECT_THAT(now(), rx_timestamp);
                EXPECT_THAT(p.size(), CANARD_MTU_MAX);
                p[0] = b('0');
                p[1] = b('1');
                p[2] = b(0b111'01101);
                return IMedia::PopResult::Metadata{rx_timestamp, 0x0C'60'23'45, 3};
            });
        scheduler_.scheduleNamedCallback("rx", rx_timestamp);

        scheduler_.scheduleAt(rx_timestamp + 1ms, [&](const TimePoint) {
            //
            const auto maybe_rx_transfer = session->receive();
            ASSERT_THAT(maybe_rx_transfer, Optional(_));
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            const auto& rx_transfer = maybe_rx_transfer.value();

            EXPECT_THAT(rx_transfer.metadata.base.timestamp, rx_timestamp);
            EXPECT_THAT(rx_transfer.metadata.base.transfer_id, 0x0D);
            EXPECT_THAT(rx_transfer.metadata.base.priority, Priority::High);
            EXPECT_THAT(rx_transfer.metadata.publisher_node_id, Optional(0x45));

            std::array<char, 2> buffer{};
            ASSERT_THAT(rx_transfer.payload.size(), buffer.size());
            EXPECT_THAT(rx_transfer.payload.copy(0, buffer.data(), buffer.size()), buffer.size());
            EXPECT_THAT(buffer, ElementsAre('0', '1'));
        });
    });
    scheduler_.scheduleAt(2s, [&](const TimePoint) {
        //
        SCOPED_TRACE("2-nd iteration: no frames available @ 2s");

        rx_timestamp = now() + 10ms;
        EXPECT_CALL(media_mock_, pop(_))  //
            .WillOnce([&](auto p) {
                EXPECT_THAT(now(), rx_timestamp);
                EXPECT_THAT(p.size(), CANARD_MTU_MAX);
                return cetl::nullopt;
            });
        scheduler_.scheduleNamedCallback("rx", rx_timestamp);

        scheduler_.scheduleAt(rx_timestamp + 1ms, [&](const TimePoint) {
            //
            const auto maybe_rx_transfer = session->receive();
            EXPECT_THAT(maybe_rx_transfer, Eq(cetl::nullopt));
        });
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestCanMsgRxSession, receive_one_anonymous_frame)
{
    auto transport = makeTransport(mr_);

    EXPECT_CALL(media_mock_, registerPopCallback(Ref(scheduler_), _))  //
        .WillOnce(Invoke([&](auto&, auto function) {                   //
            return scheduler_.registerNamedCallback("rx", std::move(function));
        }));

    auto maybe_session = transport->makeMessageRxSession({4, 0x23});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));

    EXPECT_CALL(media_mock_, setFilters(SizeIs(1)))  //
        .WillOnce([&](Filters filters) {
            EXPECT_THAT(filters, Contains(FilterEq({0x2300, 0x21FFF80})));
            return cetl::nullopt;
        });

    TimePoint rx_timestamp;

    scheduler_.scheduleAt(1s, [&](const TimePoint) {
        //
        rx_timestamp = now() + 10ms;
        EXPECT_CALL(media_mock_, pop(_))  //
            .WillOnce([&](auto p) {
                EXPECT_THAT(now(), rx_timestamp);
                EXPECT_THAT(p.size(), CANARD_MTU_MAX);
                p[0] = b('1');
                p[1] = b('2');
                p[2] = b(0b111'01110);
                return IMedia::PopResult::Metadata{rx_timestamp, 0x01'60'23'13, 3};
            });
        scheduler_.scheduleNamedCallback("rx", rx_timestamp);

        scheduler_.scheduleAt(rx_timestamp + 1ms, [&](const TimePoint) {
            //
            const auto maybe_rx_transfer = session->receive();
            ASSERT_THAT(maybe_rx_transfer, Optional(_));
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            const auto& rx_transfer = maybe_rx_transfer.value();

            EXPECT_THAT(rx_transfer.metadata.base.timestamp, rx_timestamp);
            EXPECT_THAT(rx_transfer.metadata.base.transfer_id, 0x0E);
            EXPECT_THAT(rx_transfer.metadata.base.priority, Priority::Exceptional);
            EXPECT_THAT(rx_transfer.metadata.publisher_node_id, Eq(cetl::nullopt));

            std::array<char, 2> buffer{};
            ASSERT_THAT(rx_transfer.payload.size(), buffer.size());
            EXPECT_THAT(rx_transfer.payload.copy(0, buffer.data(), buffer.size()), buffer.size());
            EXPECT_THAT(buffer, ElementsAre('1', '2'));
        });
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestCanMsgRxSession, unsubscribe)
{
    auto transport = makeTransport(mr_);

    EXPECT_CALL(media_mock_, registerPopCallback(Ref(scheduler_), _))  //
        .WillOnce(Invoke([&](auto&, auto function) {                   //
            return scheduler_.registerNamedCallback("rx", std::move(function));
        }));

    auto maybe_session = transport->makeMessageRxSession({4, 0x23});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));

    EXPECT_CALL(media_mock_, setFilters(SizeIs(1)))  //
        .WillOnce([&](Filters filters) {
            EXPECT_THAT(filters, Contains(FilterEq({0x2300, 0x21FFF80})));
            return cetl::nullopt;
        });

    scheduler_.scheduleAt(1s, [&](const TimePoint) {
        //
        EXPECT_CALL(media_mock_, setFilters(IsEmpty()))  //
            .WillOnce([&](Filters) {                     //
                return cetl::nullopt;
            });
        session.reset();
    });
    scheduler_.spinFor(10s);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
