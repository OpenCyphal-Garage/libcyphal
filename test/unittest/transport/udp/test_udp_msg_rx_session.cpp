/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../../memory_resource_mock.hpp"
#include "../../tracking_memory_resource.hpp"
#include "../../virtual_time_scheduler.hpp"
#include "../multiplexer_mock.hpp"
#include "media_mock.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/udp/media.hpp>
#include <libcyphal/transport/udp/msg_rx_session.hpp>
#include <libcyphal/transport/udp/udp_transport.hpp>
#include <libcyphal/transport/udp/udp_transport_impl.hpp>
#include <libcyphal/types.hpp>
#include <udpard.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <utility>

namespace
{

using libcyphal::TimePoint;
using libcyphal::UniquePtr;
using namespace libcyphal::transport;       // NOLINT This our main concern here in the unit tests.
using namespace libcyphal::transport::udp;  // NOLINT This our main concern here in the unit tests.

using cetl::byte;

using testing::_;
using testing::Return;
using testing::IsEmpty;
using testing::NotNull;
using testing::StrictMock;
using testing::VariantWith;

// https://github.com/llvm/llvm-project/issues/53444
// NOLINTBEGIN(misc-unused-using-decls, misc-include-cleaner)
using std::literals::chrono_literals::operator""s;
using std::literals::chrono_literals::operator""ms;
// NOLINTEND(misc-unused-using-decls, misc-include-cleaner)

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestUdpMsgRxSession : public testing::Test
{
protected:
    void SetUp() override
    {
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

    UniquePtr<IUdpTransport> makeTransport(const MemoryResourcesSpec& mem_res_spec)
    {
        std::array<IMedia*, 1> media_array{&media_mock_};

        auto maybe_transport = udp::makeTransport(mem_res_spec, mux_mock_, media_array, 0);
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<IUdpTransport>>(NotNull()));
        return cetl::get<UniquePtr<IUdpTransport>>(std::move(maybe_transport));
    }

    // MARK: Data members:

    // NOLINTBEGIN
    libcyphal::VirtualTimeScheduler scheduler_{};
    TrackingMemoryResource          mr_;
    StrictMock<MultiplexerMock>     mux_mock_{};
    StrictMock<MediaMock>           media_mock_{};
    // NOLINTEND
};

// MARK: Tests:

TEST_F(TestUdpMsgRxSession, make_setTransferIdTimeout)
{
    auto transport = makeTransport({mr_});

    auto maybe_session = transport->makeMessageRxSession({42, 123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));

    EXPECT_THAT(session->getParams().extent_bytes, 42);
    EXPECT_THAT(session->getParams().subject_id, 123);

    session->setTransferIdTimeout(0s);
    session->setTransferIdTimeout(500ms);
}

TEST_F(TestUdpMsgRxSession, make_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    // Emulate that there is no memory available for the message session.
    EXPECT_CALL(mr_mock, do_allocate(sizeof(udp::detail::MessageRxSession), _)).WillOnce(Return(nullptr));

    auto transport = makeTransport({mr_mock});

    auto maybe_session = transport->makeMessageRxSession({64, 0x23});
    EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<MemoryError>(_)));
}

TEST_F(TestUdpMsgRxSession, make_fails_due_to_argument_error)
{
    auto transport = makeTransport({mr_});

    // Try invalid subject id
    auto maybe_session = transport->makeMessageRxSession({64, UDPARD_SUBJECT_ID_MAX + 1});
    EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<ArgumentError>(_)));
}

// TODO: Uncomment gradually as the implementation progresses.
/*
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_F(TestUdpMsgRxSession, run_and_receive)
{
    auto transport = makeTransport({mr_});

    auto maybe_session = transport->makeMessageRxSession({4, 0x23});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));

    {
        SCOPED_TRACE("1-st iteration: one frame available @ 1s");

        scheduler_.setNow(TimePoint{1s});
        const auto rx_timestamp = now();

        EXPECT_CALL(media_mock_, pop(_)).WillOnce([&](auto p) {
            EXPECT_THAT(now(), rx_timestamp + 10ms);
            EXPECT_THAT(p.size(), UDPARD_MTU_DEFAULT);
            p[0] = b('0');
            p[1] = b('1');
            p[2] = b(0b111'01101);
            return RxMetadata{rx_timestamp, 0x0C'60'23'45, 3};
        });
        EXPECT_CALL(media_mock_, setFilters(SizeIs(1))).WillOnce([&](Filters filters) {
            EXPECT_THAT(now(), rx_timestamp + 10ms);
            EXPECT_THAT(filters, Contains(FilterEq({0x2300, 0x21FFF80})));
            return cetl::nullopt;
        });

        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(session->run(now()), UbVariantWithoutValue()); });

        const auto maybe_rx_transfer = session->receive();
        ASSERT_THAT(maybe_rx_transfer, Optional(_));
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
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
            EXPECT_THAT(p.size(), UDPARD_MTU_DEFAULT);
            return cetl::nullopt;
        });

        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(session->run(now()), UbVariantWithoutValue()); });

        const auto maybe_rx_transfer = session->receive();
        EXPECT_THAT(maybe_rx_transfer, Eq(cetl::nullopt));
    }
}

TEST_F(TestUdpMsgRxSession, run_and_receive_one_anonymous_frame)
{
    auto transport = makeTransport({mr_});

    auto maybe_session = transport->makeMessageRxSession({4, 0x23});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));

    scheduler_.setNow(TimePoint{1s});
    const auto rx_timestamp = now();

    {
        const InSequence seq;

        EXPECT_CALL(media_mock_, pop(_)).WillOnce([&](auto p) {
            EXPECT_THAT(now(), rx_timestamp + 10ms);
            EXPECT_THAT(p.size(), UDPARD_MTU_DEFAULT);
            p[0] = b('1');
            p[1] = b('2');
            p[2] = b(0b111'01110);
            return RxMetadata{rx_timestamp, 0x01'60'23'13, 3};
        });
        EXPECT_CALL(media_mock_, setFilters(SizeIs(1))).WillOnce([&](Filters filters) {
            EXPECT_THAT(now(), rx_timestamp + 10ms);
            EXPECT_THAT(filters, Contains(FilterEq({0x2300, 0x21FFF80})));
            return cetl::nullopt;
        });
    }

    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(session->run(now()), UbVariantWithoutValue()); });

    const auto maybe_rx_transfer = session->receive();
    ASSERT_THAT(maybe_rx_transfer, Optional(_));
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
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

TEST_F(TestUdpMsgRxSession, unsubscribe_and_run)
{
    auto transport = makeTransport({mr_});

    auto maybe_session = transport->makeMessageRxSession({4, 0x23});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));

    scheduler_.setNow(TimePoint{1s});
    const auto reset_time = now();

    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));
    EXPECT_CALL(media_mock_, setFilters(IsEmpty())).WillOnce([&](Filters) {
        EXPECT_THAT(now(), reset_time + 10ms);
        return cetl::nullopt;
    });

    session.reset();

    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}
*/

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
