/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../../cetl_gtest_helpers.hpp"
#include "../../executor_mock.hpp"
#include "../../memory_resource_mock.hpp"
#include "../../tracking_memory_resource.hpp"
#include "../../verification_utilities.hpp"
#include "../../virtual_time_scheduler.hpp"
#include "can_gtest_helpers.hpp"
#include "media_mock.hpp"
#include "transient_error_handler_mock.hpp"

#include <canard.h>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/rtti.hpp>
#include <libcyphal/transport/can/can_transport.hpp>
#include <libcyphal/transport/can/can_transport_impl.hpp>
#include <libcyphal/transport/can/media.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/svc_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <utility>

namespace
{

using libcyphal::TimePoint;
using libcyphal::UniquePtr;
using namespace libcyphal::transport;       // NOLINT This our main concern here in the unit tests.
using namespace libcyphal::transport::can;  // NOLINT This our main concern here in the unit tests.

using libcyphal::verification_utilities::b;
using libcyphal::verification_utilities::makeIotaArray;
using libcyphal::verification_utilities::makeSpansFrom;

using testing::_;
using testing::Eq;
using testing::Ref;
using testing::Truly;
using testing::Return;
using testing::SizeIs;
using testing::IsEmpty;
using testing::NotNull;
using testing::Contains;
using testing::Optional;
using testing::InSequence;
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

class MyPlatformError final : public libcyphal::transport::IPlatformError
{
public:
    explicit MyPlatformError(std::uint32_t code)
        : code_{code}
    {
    }
    virtual ~MyPlatformError() noexcept                    = default;
    MyPlatformError(const MyPlatformError&)                = default;
    MyPlatformError(MyPlatformError&&) noexcept            = default;
    MyPlatformError& operator=(const MyPlatformError&)     = default;
    MyPlatformError& operator=(MyPlatformError&&) noexcept = default;

    // MARK: IPlatformError

    std::uint32_t code() const noexcept override
    {
        return code_;
    }

private:
    std::uint32_t code_;

};  // MyPlatformError

class TestCanTransport : public testing::Test
{
protected:
    void SetUp() override
    {
        EXPECT_CALL(media_mock_, getMtu()).WillRepeatedly(Return(CANARD_MTU_CAN_CLASSIC));
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

    UniquePtr<ICanTransport> makeTransport(cetl::pmr::memory_resource& mr,
                                           IMedia*                     extra_media = nullptr,
                                           const std::size_t           tx_capacity = 16)
    {
        std::array<IMedia*, 2> media_array{&media_mock_, extra_media};

        auto maybe_transport = can::makeTransport(mr, executor_mock_, media_array, tx_capacity);
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));
        return cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
    }

    // MARK: Data members:

    // NOLINTBEGIN
    libcyphal::VirtualTimeScheduler     scheduler_{};
    TrackingMemoryResource              mr_;
    StrictMock<libcyphal::ExecutorMock> executor_mock_{};
    StrictMock<MediaMock>               media_mock_{};
    // NOLINTEND
};

// MARK: Tests:

TEST_F(TestCanTransport, makeTransport_no_memory_at_all)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    // Emulate that there is no memory at all (even for initial array of media).
    EXPECT_CALL(mr_mock, do_allocate(_, _)).WillRepeatedly(Return(nullptr));
#if (__cplusplus < CETL_CPP_STANDARD_17)
    EXPECT_CALL(mr_mock, do_reallocate(nullptr, 0, _, _)).WillRepeatedly(Return(nullptr));
#endif

    std::array<IMedia*, 1> media_array{&media_mock_};
    auto                   maybe_transport = can::makeTransport(mr_mock, executor_mock_, media_array, 0);
    EXPECT_THAT(maybe_transport, VariantWith<FactoryFailure>(VariantWith<MemoryError>(_)));
}

TEST_F(TestCanTransport, makeTransport_no_memory_for_impl)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    // Emulate that there is no memory available for the transport.
    EXPECT_CALL(mr_mock, do_allocate(sizeof(can::detail::TransportImpl), _)).WillOnce(Return(nullptr));

    std::array<IMedia*, 1> media_array{&media_mock_};
    auto                   maybe_transport = can::makeTransport(mr_mock, executor_mock_, media_array, 0);
    EXPECT_THAT(maybe_transport, VariantWith<FactoryFailure>(VariantWith<MemoryError>(_)));
}

TEST_F(TestCanTransport, makeTransport_too_many_media)
{
    // Canard use `std::uint8_t` as a media index, so 256+ media interfaces are not allowed.
    std::array<IMedia*, std::numeric_limits<std::uint8_t>::max() + 1> media_array{};
    std::fill(media_array.begin(), media_array.end(), &media_mock_);

    auto maybe_transport = can::makeTransport(mr_, executor_mock_, media_array, 0);
    EXPECT_THAT(maybe_transport, VariantWith<FactoryFailure>(VariantWith<ArgumentError>(_)));
}

TEST_F(TestCanTransport, makeTransport_getLocalNodeId)
{
    // Anonymous node
    {
        std::array<IMedia*, 1> media_array{&media_mock_};
        auto                   maybe_transport = can::makeTransport(mr_, executor_mock_, media_array, 0);
        ASSERT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));

        auto transport = cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
        EXPECT_THAT(transport->getLocalNodeId(), Eq(cetl::nullopt));
    }

    // Node with ID
    {
        std::array<IMedia*, 1> media_array{&media_mock_};
        auto                   maybe_transport = can::makeTransport(mr_, executor_mock_, media_array, 0);
        ASSERT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));

        auto transport = cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
        transport->setLocalNodeId(42);

        EXPECT_THAT(transport->getLocalNodeId(), Optional(42));
    }

    // Two media interfaces
    {
        StrictMock<MediaMock> media_mock2;
        EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_MAX));

        std::array<IMedia*, 3> media_array{&media_mock_, nullptr, &media_mock2};
        auto                   maybe_transport = can::makeTransport(mr_, executor_mock_, media_array, 0);
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));
    }

    // All 3 maximum number of media interfaces
    {
        StrictMock<MediaMock> media_mock2{};
        StrictMock<MediaMock> media_mock3{};
        EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_MAX));
        EXPECT_CALL(media_mock3, getMtu()).WillRepeatedly(Return(CANARD_MTU_MAX));

        std::array<IMedia*, 3> media_array{&media_mock_, &media_mock2, &media_mock3};
        auto                   maybe_transport = can::makeTransport(mr_, executor_mock_, media_array, 0);
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));
    }
}

TEST_F(TestCanTransport, setLocalNodeId)
{
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));

    auto transport = makeTransport(mr_);

    EXPECT_THAT(transport->setLocalNodeId(CANARD_NODE_ID_MAX + 1), Optional(testing::A<ArgumentError>()));
    EXPECT_THAT(transport->getLocalNodeId(), Eq(cetl::nullopt));

    EXPECT_THAT(transport->run(now()), UbVariantWithoutValue());

    EXPECT_THAT(transport->setLocalNodeId(CANARD_NODE_ID_MAX), Eq(cetl::nullopt));
    EXPECT_THAT(transport->getLocalNodeId(), Optional(CANARD_NODE_ID_MAX));

    EXPECT_THAT(transport->run(now()), UbVariantWithoutValue());

    EXPECT_THAT(transport->setLocalNodeId(CANARD_NODE_ID_MAX), Eq(cetl::nullopt));
    EXPECT_THAT(transport->getLocalNodeId(), Optional(CANARD_NODE_ID_MAX));

    EXPECT_THAT(transport->run(now()), UbVariantWithoutValue());

    EXPECT_THAT(transport->setLocalNodeId(0), Optional(testing::A<ArgumentError>()));
    EXPECT_THAT(transport->getLocalNodeId(), Optional(CANARD_NODE_ID_MAX));

    EXPECT_THAT(transport->run(now()), UbVariantWithoutValue());
}

TEST_F(TestCanTransport, makeTransport_with_invalid_arguments)
{
    // No media
    const auto maybe_transport = can::makeTransport(mr_, executor_mock_, {}, 0);
    EXPECT_THAT(maybe_transport, VariantWith<FactoryFailure>(VariantWith<ArgumentError>(_)));
}

TEST_F(TestCanTransport, getProtocolParams)
{
    StrictMock<MediaMock> media_mock2{};
    EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_MAX));

    std::array<IMedia*, 2> media_array{&media_mock_, &media_mock2};
    auto transport = cetl::get<UniquePtr<ICanTransport>>(can::makeTransport(mr_, executor_mock_, media_array, 0));

    EXPECT_CALL(media_mock_, getMtu()).WillRepeatedly(Return(CANARD_MTU_CAN_FD));
    EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_CAN_CLASSIC));

    auto params = transport->getProtocolParams();
    EXPECT_THAT(params.transfer_id_modulo, static_cast<TransferId>(1) << CANARD_TRANSFER_ID_BIT_LENGTH);
    EXPECT_THAT(params.max_nodes, CANARD_NODE_ID_MAX + 1);
    EXPECT_THAT(params.mtu_bytes, CANARD_MTU_CAN_CLASSIC);

    // Manipulate MTU values on fly
    {
        EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_CAN_FD));
        EXPECT_THAT(transport->getProtocolParams().mtu_bytes, CANARD_MTU_CAN_FD);

        EXPECT_CALL(media_mock_, getMtu()).WillRepeatedly(Return(CANARD_MTU_CAN_CLASSIC));
        EXPECT_THAT(transport->getProtocolParams().mtu_bytes, CANARD_MTU_CAN_CLASSIC);

        EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_CAN_CLASSIC));
        EXPECT_THAT(transport->getProtocolParams().mtu_bytes, CANARD_MTU_CAN_CLASSIC);
    }
}

TEST_F(TestCanTransport, makeMessageRxSession)
{
    auto transport = makeTransport(mr_);

    auto maybe_rx_session = transport->makeMessageRxSession({42, 123});
    ASSERT_THAT(maybe_rx_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));

    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_rx_session));
    EXPECT_THAT(session->getParams().extent_bytes, 42);
    EXPECT_THAT(session->getParams().subject_id, 123);
}

TEST_F(TestCanTransport, makeMessageRxSession_invalid_subject_id)
{
    auto transport = makeTransport(mr_);

    auto maybe_rx_session = transport->makeMessageRxSession({0, CANARD_SUBJECT_ID_MAX + 1});
    EXPECT_THAT(maybe_rx_session, VariantWith<AnyFailure>(VariantWith<ArgumentError>(_)));
}

TEST_F(TestCanTransport, makeMessageRxSession_invalid_resubscription)
{
    auto transport = makeTransport(mr_);

    const PortId test_subject_id = 111;

    auto maybe_rx_session1 = transport->makeMessageRxSession({0, test_subject_id});
    ASSERT_THAT(maybe_rx_session1, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));

    auto maybe_rx_session2 = transport->makeMessageRxSession({0, test_subject_id});
    EXPECT_THAT(maybe_rx_session2, VariantWith<AnyFailure>(VariantWith<AlreadyExistsError>(_)));

    // Now release the first session and try to subscribe again - should succeed.
    {
        auto rx_session1 = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_rx_session1));
        rx_session1.reset();

        maybe_rx_session2 = transport->makeMessageRxSession({0, test_subject_id});
        ASSERT_THAT(maybe_rx_session2, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));
    }
}

TEST_F(TestCanTransport, makeRequestRxSession_invalid_resubscription)
{
    auto transport = makeTransport(mr_);

    const PortId test_subject_id = 111;

    auto maybe_rx_session1 = transport->makeRequestRxSession({0, test_subject_id});
    ASSERT_THAT(maybe_rx_session1, VariantWith<UniquePtr<IRequestRxSession>>(NotNull()));

    auto maybe_rx_session2 = transport->makeRequestRxSession({0, test_subject_id});
    EXPECT_THAT(maybe_rx_session2, VariantWith<AnyFailure>(VariantWith<AlreadyExistsError>(_)));

    // Now release the first session and try to subscribe again - should succeed.
    {
        auto rx_session1 = cetl::get<UniquePtr<IRequestRxSession>>(std::move(maybe_rx_session1));
        rx_session1.reset();

        maybe_rx_session2 = transport->makeRequestRxSession({0, test_subject_id});
        ASSERT_THAT(maybe_rx_session2, VariantWith<UniquePtr<IRequestRxSession>>(NotNull()));
    }
}

TEST_F(TestCanTransport, makeResponseRxSession_invalid_resubscription)
{
    auto transport = makeTransport(mr_);

    const PortId test_subject_id = 111;

    auto maybe_rx_session1 = transport->makeResponseRxSession({0, test_subject_id, 0x31});
    ASSERT_THAT(maybe_rx_session1, VariantWith<UniquePtr<IResponseRxSession>>(NotNull()));

    auto maybe_rx_session2 = transport->makeResponseRxSession({0, test_subject_id, 0x31});
    EXPECT_THAT(maybe_rx_session2, VariantWith<AnyFailure>(VariantWith<AlreadyExistsError>(_)));

    // Now release the first session and try to subscribe again - should succeed.
    {
        auto rx_session1 = cetl::get<UniquePtr<IResponseRxSession>>(std::move(maybe_rx_session1));
        rx_session1.reset();

        maybe_rx_session2 = transport->makeResponseRxSession({0, test_subject_id, 0x31});
        ASSERT_THAT(maybe_rx_session2, VariantWith<UniquePtr<IResponseRxSession>>(NotNull()));
    }
}

TEST_F(TestCanTransport, makeMessageTxSession)
{
    auto transport = makeTransport(mr_);

    auto maybe_tx_session = transport->makeMessageTxSession({123});
    ASSERT_THAT(maybe_tx_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));

    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_tx_session));
    EXPECT_THAT(session->getParams().subject_id, 123);
}

TEST_F(TestCanTransport, sending_multiframe_payload_should_fail_for_anonymous)
{
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));

    auto transport = makeTransport(mr_);

    auto maybe_session = transport->makeMessageTxSession({7});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    scheduler_.runNow(+10s);
    const auto send_time = now();

    const auto             payload = makeIotaArray<CANARD_MTU_CAN_CLASSIC>(b('0'));
    const TransferMetadata metadata{0x13, send_time, Priority::Nominal};

    auto failure = session->send(metadata, makeSpansFrom(payload));
    EXPECT_THAT(failure, Optional(VariantWith<ArgumentError>(_)));

    scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(10us, [&] { EXPECT_THAT(session->run(now()), UbVariantWithoutValue()); });
}

TEST_F(TestCanTransport, sending_multiframe_payload_for_non_anonymous)
{
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));

    auto transport = makeTransport(mr_);
    EXPECT_THAT(transport->setLocalNodeId(0x45), Eq(cetl::nullopt));

    auto maybe_session = transport->makeMessageTxSession({7});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    scheduler_.runNow(+10s);
    const auto timeout   = 1s;
    const auto send_time = now();

    const auto             payload = makeIotaArray<CANARD_MTU_CAN_CLASSIC>(b('0'));
    const TransferMetadata metadata{0x13, send_time, Priority::Nominal};

    auto failure = session->send(metadata, makeSpansFrom(payload));
    EXPECT_THAT(failure, Eq(cetl::nullopt));

    {
        const InSequence s;

        EXPECT_CALL(media_mock_, push(_, _, _)).WillOnce([&](auto deadline, auto can_id, auto payload) {
            EXPECT_THAT(now(), send_time + 10us);
            EXPECT_THAT(deadline, send_time + timeout);
            EXPECT_THAT(can_id, AllOf(SubjectOfCanIdEq(7), SourceNodeOfCanIdEq(0x45)));
            EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsMessageCanId()));

            auto tbm = TailByteEq(metadata.transfer_id, true, false);
            EXPECT_THAT(payload, ElementsAre(b('0'), b('1'), b('2'), b('3'), b('4'), b('5'), b('6'), tbm));
            return IMedia::PushResult::Success{true /*is_accepted*/};
        });
        EXPECT_CALL(media_mock_, push(_, _, _)).WillOnce([&](auto deadline, auto can_id, auto payload) {
            EXPECT_THAT(now(), send_time + 10us);
            EXPECT_THAT(deadline, send_time + timeout);
            EXPECT_THAT(can_id, AllOf(SubjectOfCanIdEq(7), SourceNodeOfCanIdEq(0x45)));
            EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsMessageCanId()));

            auto tbm = TailByteEq(metadata.transfer_id, false, true, false);
            EXPECT_THAT(payload, ElementsAre(b('7'), _, _ /* CRC bytes */, tbm));
            return IMedia::PushResult::Success{true /*is_accepted*/};
        });
    }

    scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_F(TestCanTransport, send_multiframe_payload_to_redundant_not_ready_media)
{
    StrictMock<MediaMock> media_mock2{};
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));
    EXPECT_CALL(media_mock2, pop(_)).WillRepeatedly(Return(cetl::nullopt));
    EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_CAN_CLASSIC));

    auto transport = makeTransport(mr_, &media_mock2);
    EXPECT_THAT(transport->setLocalNodeId(0x45), Eq(cetl::nullopt));

    auto maybe_session = transport->makeMessageTxSession({7});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    scheduler_.runNow(+10s);
    const auto timeout   = 1s;
    const auto send_time = now();

    const auto             payload = makeIotaArray<10>(b('0'));
    const TransferMetadata metadata{0x13, send_time, Priority::Nominal};

    auto failure = session->send(metadata, makeSpansFrom(payload));
    EXPECT_THAT(failure, Eq(cetl::nullopt));

    {
        const InSequence s;

        auto expectMediaCalls = [&](MediaMock& media_mock, const std::string& ctx, TimePoint when) {
            EXPECT_CALL(media_mock, push(_, _, _)).WillOnce([&, ctx, when](auto deadline, auto can_id, auto payload) {
                EXPECT_THAT(now(), when) << ctx;
                EXPECT_THAT(deadline, send_time + timeout) << ctx;
                EXPECT_THAT(can_id, AllOf(SubjectOfCanIdEq(7), SourceNodeOfCanIdEq(0x45))) << ctx;
                EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsMessageCanId())) << ctx;

                auto tbm = TailByteEq(metadata.transfer_id, true, false);
                EXPECT_THAT(payload, ElementsAre(b('0'), b('1'), b('2'), b('3'), b('4'), b('5'), b('6'), tbm)) << ctx;
                return IMedia::PushResult::Success{true /*is_accepted*/};
            });
            EXPECT_CALL(media_mock, push(_, _, _)).WillOnce([&, ctx, when](auto deadline, auto can_id, auto payload) {
                EXPECT_THAT(now(), when) << ctx;
                EXPECT_THAT(deadline, send_time + timeout) << ctx;
                EXPECT_THAT(can_id, AllOf(SubjectOfCanIdEq(7), SourceNodeOfCanIdEq(0x45))) << ctx;
                EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsMessageCanId())) << ctx;

                auto tbm = TailByteEq(metadata.transfer_id, false, true, false);
                EXPECT_THAT(payload, ElementsAre(b('7'), b('8'), b('9'), b(0x7D), b(0x61) /* CRC bytes */, tbm)) << ctx;
                return IMedia::PushResult::Success{true /*is_accepted*/};
            });
        };

        // Emulate once that the first media is not ready to push fragment (@10us). So transport will
        // switch to the second media, and only on the next run will (@20us) retry with the first media again.
        //
        EXPECT_CALL(media_mock_, push(_, _, _)).WillOnce([&](auto, auto, auto) {
            EXPECT_THAT(now(), send_time + 10us);
            return IMedia::PushResult::Success{false /*is_accepted*/};
        });
        expectMediaCalls(media_mock2, "M#2", send_time + 10us);
        expectMediaCalls(media_mock_, "M#1", send_time + 20us);
    }

    scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}

TEST_F(TestCanTransport, send_payload_to_redundant_fallible_media)
{
    using MediaPushReport = ICanTransport::TransientErrorReport::MediaPush;

    StrictMock<MediaMock> media_mock2{};
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));
    EXPECT_CALL(media_mock2, pop(_)).WillRepeatedly(Return(cetl::nullopt));
    EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_CAN_CLASSIC));

    auto transport = makeTransport(mr_, &media_mock2);
    EXPECT_THAT(transport->setLocalNodeId(0x45), Eq(cetl::nullopt));

    auto maybe_session = transport->makeMessageTxSession({7});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    scheduler_.runNow(+10s);
    const auto send_time = now();

    const auto             payload = makeIotaArray<6>(b('0'));
    const TransferMetadata metadata{0x13, send_time, Priority::Nominal};

    // First attempt to send payload.
    auto failure = session->send(metadata, makeSpansFrom(payload));
    EXPECT_THAT(failure, Eq(cetl::nullopt));

    // 1st run: media #0 and there is no transient error handler; its frame should be dropped.
    {
        EXPECT_CALL(media_mock_, push(_, _, _)).WillOnce(Return(CapacityError{}));

        scheduler_.runNow(+10us, [&] {
            EXPECT_THAT(transport->run(now()), UbVariantWith<AnyFailure>(VariantWith<CapacityError>(_)));
        });
    }
    // 2nd run: media #1 and transient error handler have failed; its frame should be dropped.
    {
        StrictMock<TransientErrorHandlerMock> handler_mock{};
        transport->setTransientErrorHandler(std::ref(handler_mock));
        EXPECT_CALL(handler_mock, invoke(VariantWith<MediaPushReport>(Truly([&](auto& report) {
                        EXPECT_THAT(report.failure, VariantWith<CapacityError>(_));
                        EXPECT_THAT(report.media_index, 1);
                        EXPECT_THAT(report.culprit, Ref(media_mock2));
                        return true;
                    }))))
            .WillOnce(Return(CapacityError{}));

        EXPECT_CALL(media_mock2, push(_, _, _)).WillOnce(Return(CapacityError{}));

        scheduler_.runNow(+10us, [&] {
            EXPECT_THAT(transport->run(now()), UbVariantWith<AnyFailure>(VariantWith<CapacityError>(_)));
        });

        // No frames should be left in the session.
        scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    }

    // Second attempt to send payload.
    EXPECT_THAT(session->send(metadata, makeSpansFrom(payload)), Eq(cetl::nullopt));

    // 3rd run: media #0 has failed but transient error handler succeeded.
    {
        StrictMock<TransientErrorHandlerMock> handler_mock{};
        transport->setTransientErrorHandler(std::ref(handler_mock));
        EXPECT_CALL(handler_mock, invoke(VariantWith<MediaPushReport>(Truly([&](auto& report) {
                        EXPECT_THAT(report.failure, VariantWith<CapacityError>(_));
                        EXPECT_THAT(report.media_index, 0);
                        EXPECT_THAT(report.culprit, Ref(media_mock_));
                        return true;
                    }))))
            .WillOnce(Return(cetl::nullopt));

        EXPECT_CALL(media_mock_, push(_, _, _)).WillOnce(Return(CapacityError{}));
        EXPECT_CALL(media_mock2, push(_, _, _)).WillOnce(Return(IMedia::PushResult::Success{true /*is_accepted*/}));

        scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });

        // No frames should be left in the session.
        scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    }
}

TEST_F(TestCanTransport, send_payload_to_out_of_capacity_canard_tx)
{
    using CanardTxPushReport = ICanTransport::TransientErrorReport::CanardTxPush;

    StrictMock<MediaMock> media_mock2{};
    EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_CAN_CLASSIC));

    // Make transport with no TX capacity - this will cause `MemoryError` on send attempts.
    //
    auto transport = makeTransport(mr_, &media_mock2, 0 /*capacity*/);
    EXPECT_THAT(transport->setLocalNodeId(0x45), Eq(cetl::nullopt));

    auto maybe_session = transport->makeMessageTxSession({7});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    const auto             payload = makeIotaArray<6>(b('0'));
    const TransferMetadata metadata{0x13, now(), Priority::Nominal};

    // 1st. Try to send a frame with "failing" handler - only the 0-th media index will be used.
    {
        StrictMock<TransientErrorHandlerMock> handler_mock{};
        transport->setTransientErrorHandler(std::ref(handler_mock));
        EXPECT_CALL(handler_mock, invoke(VariantWith<CanardTxPushReport>(Truly([](auto& report) {
                        EXPECT_THAT(report.failure, VariantWith<MemoryError>(_));
                        EXPECT_THAT(report.media_index, 0);
                        EXPECT_THAT(report.culprit, Truly([](auto& canard_ins) {
                                        EXPECT_THAT(canard_ins.node_id, 0x45);
                                        return true;
                                    }));
                        return true;
                    }))))
            .WillOnce(Return(StateError{}));

        auto failure = session->send(metadata, makeSpansFrom(payload));
        EXPECT_THAT(failure, Optional(VariantWith<StateError>(_)));
    }
    // 2nd. Try to send a frame with "succeeding" handler - both media indices will be used.
    {
        StrictMock<TransientErrorHandlerMock> handler_mock{};
        transport->setTransientErrorHandler(std::ref(handler_mock));
        EXPECT_CALL(handler_mock, invoke(VariantWith<CanardTxPushReport>(Truly([](auto& report) {
                        EXPECT_THAT(report.failure, VariantWith<MemoryError>(_));
                        return report.media_index == 0;
                    }))))
            .WillOnce(Return(cetl::nullopt));
        EXPECT_CALL(handler_mock, invoke(VariantWith<CanardTxPushReport>(Truly([](auto& report) {
                        EXPECT_THAT(report.failure, VariantWith<MemoryError>(_));
                        return report.media_index == 1;
                    }))))
            .WillOnce(Return(cetl::nullopt));

        auto failure = session->send(metadata, makeSpansFrom(payload));
        EXPECT_THAT(failure, Eq(cetl::nullopt));
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_F(TestCanTransport, run_and_receive_svc_responses_from_redundant_media)
{
    StrictMock<MediaMock> media_mock2{};
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));
    EXPECT_CALL(media_mock2, pop(_)).WillRepeatedly(Return(cetl::nullopt));
    EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_CAN_CLASSIC));

    auto transport = makeTransport(mr_, &media_mock2);
    EXPECT_THAT(transport->setLocalNodeId(0x13), Eq(cetl::nullopt));

    auto maybe_session = transport->makeResponseRxSession({64, 0x17B, 0x31});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IResponseRxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IResponseRxSession>>(std::move(maybe_session));

    const auto params = session->getParams();
    EXPECT_THAT(params.extent_bytes, 64);
    EXPECT_THAT(params.service_id, 0x17B);
    EXPECT_THAT(params.server_node_id, 0x31);

    const auto timeout = 200ms;
    session->setTransferIdTimeout(timeout);

    {
        EXPECT_CALL(media_mock_, setFilters(SizeIs(1))).WillOnce([&](Filters filters) {
            EXPECT_THAT(now(), TimePoint{0s} + 10us);
            EXPECT_THAT(filters, Contains(FilterEq({0x25EC980, 0x2FFFF80})));
            return cetl::nullopt;
        });
        EXPECT_CALL(media_mock2, setFilters(SizeIs(1))).WillOnce([&](Filters filters) {
            EXPECT_THAT(now(), TimePoint{0s} + 10us);
            EXPECT_THAT(filters, Contains(FilterEq({0x25EC980, 0x2FFFF80})));
            return cetl::nullopt;
        });

        scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    }

    const auto epoch = TimePoint{10s};
    scheduler_.setNow(epoch);
    const auto rx1_timestamp = epoch;
    const auto rx2_timestamp = epoch + 2 * timeout;
    {
        const InSequence seq;

        // 1. Emulate that only one 1st frame came from the 1st media interface (@ rx1_timestamp+10ms)...
        //
        EXPECT_CALL(media_mock_, pop(_)).WillOnce([&](auto p) {
            EXPECT_THAT(now(), rx1_timestamp + 10ms);
            EXPECT_THAT(p.size(), CANARD_MTU_MAX);
            p[0] = b('0');
            p[1] = b('1');
            p[2] = b('2');
            p[3] = b('3');
            p[4] = b('4');
            p[5] = b('5');
            p[6] = b('6');
            p[7] = b(0b101'11101);
            return IMedia::PopResult::Metadata{rx1_timestamp, 0b111'1'0'0'101111011'0010011'0110001, 8};
        });
        EXPECT_CALL(media_mock2, pop(_)).WillOnce([&](auto) {
            EXPECT_THAT(now(), rx1_timestamp + 10ms);
            return cetl::nullopt;
        });
        // 2. And then 2nd media delivered all frames ones again after timeout (@ rx2_timestamp+10ms).
        //
        EXPECT_CALL(media_mock_, pop(_)).WillOnce([&](auto) {
            EXPECT_THAT(now(), rx2_timestamp + 10ms);
            return cetl::nullopt;
        });
        EXPECT_CALL(media_mock2, pop(_)).WillOnce([&](auto p) {
            EXPECT_THAT(now(), rx2_timestamp + 10ms);
            EXPECT_THAT(p.size(), CANARD_MTU_MAX);
            p[0] = b('0');
            p[1] = b('1');
            p[2] = b('2');
            p[3] = b('3');
            p[4] = b('4');
            p[5] = b('5');
            p[6] = b('6');
            p[7] = b(0b101'11110);
            return IMedia::PopResult::Metadata{rx2_timestamp, 0b111'1'0'0'101111011'0010011'0110001, 8};
        });
        EXPECT_CALL(media_mock2, pop(_)).WillOnce([&](auto p) {
            EXPECT_THAT(now(), rx2_timestamp + 30ms);
            EXPECT_THAT(p.size(), CANARD_MTU_MAX);
            p[0] = b('7');
            p[1] = b('8');
            p[2] = b('9');
            p[3] = b(0x7D);
            p[4] = b(0x61);  // expected 16-bit CRC
            p[5] = b(0b010'11110);
            return IMedia::PopResult::Metadata{rx2_timestamp + 1ms, 0b111'1'0'0'101111011'0010011'0110001, 6};
        });
    }
    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(session->run(now()), UbVariantWithoutValue()); });
    scheduler_.setNow(rx2_timestamp);
    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(session->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(session->run(now()), UbVariantWithoutValue()); });

    const auto maybe_rx_transfer = session->receive();
    ASSERT_THAT(maybe_rx_transfer, Optional(_));
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    const auto& rx_transfer = maybe_rx_transfer.value();

    EXPECT_THAT(rx_transfer.metadata.timestamp, rx2_timestamp);
    EXPECT_THAT(rx_transfer.metadata.transfer_id, 0x1E);
    EXPECT_THAT(rx_transfer.metadata.priority, Priority::Optional);
    EXPECT_THAT(rx_transfer.metadata.remote_node_id, 0x31);

    std::array<char, 10> buffer{};
    EXPECT_THAT(rx_transfer.payload.size(), buffer.size());
    EXPECT_THAT(rx_transfer.payload.copy(0, buffer.data(), buffer.size()), buffer.size());
    EXPECT_THAT(buffer, ElementsAre('0', '1', '2', '3', '4', '5', '6', '7', '8', '9'));

    {
        SCOPED_TRACE("unsubscribe @ 99s");

        scheduler_.setNow(TimePoint{99s});
        const auto reset_time = now();

        EXPECT_CALL(media_mock_, setFilters(IsEmpty())).WillOnce([&](Filters) {
            EXPECT_THAT(now(), reset_time + 10ms);
            return cetl::nullopt;
        });
        EXPECT_CALL(media_mock2, setFilters(IsEmpty())).WillOnce([&](Filters) {
            EXPECT_THAT(now(), reset_time + 10ms);
            return cetl::nullopt;
        });
        EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));
        EXPECT_CALL(media_mock2, pop(_)).WillRepeatedly(Return(cetl::nullopt));

        session.reset();

        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    }
}

TEST_F(TestCanTransport, run_and_receive_svc_responses_from_redundant_fallible_media)
{
    using MediaPopReport = ICanTransport::TransientErrorReport::MediaPop;

    StrictMock<MediaMock> media_mock2{};
    EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_CAN_CLASSIC));

    auto transport = makeTransport(mr_, &media_mock2);
    EXPECT_THAT(transport->setLocalNodeId(0x13), Eq(cetl::nullopt));

    auto maybe_session = transport->makeResponseRxSession({64, 0x17B, 0x31});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IResponseRxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IResponseRxSession>>(std::move(maybe_session));

    // skip `setFilters` calls; they are tested elsewhere.
    {
        EXPECT_CALL(media_mock_, pop(_)).WillOnce(Return(cetl::nullopt));
        EXPECT_CALL(media_mock2, pop(_)).WillOnce(Return(cetl::nullopt));

        EXPECT_CALL(media_mock_, setFilters(_)).WillOnce(Return(cetl::nullopt));
        EXPECT_CALL(media_mock2, setFilters(_)).WillOnce(Return(cetl::nullopt));

        scheduler_.runNow(+0us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    }

    // 1st run: media #0 pop has failed and there is no transient error handler.
    {
        EXPECT_CALL(media_mock_, pop(_)).WillOnce(Return(ArgumentError{}));

        scheduler_.runNow(+1s, [&] {
            EXPECT_THAT(transport->run(now()), UbVariantWith<AnyFailure>(VariantWith<ArgumentError>(_)));
        });
    }
    // 2nd run: media #0 pop and transient error handler have failed.
    {
        StrictMock<TransientErrorHandlerMock> handler_mock{};
        transport->setTransientErrorHandler(std::ref(handler_mock));
        EXPECT_CALL(handler_mock, invoke(VariantWith<MediaPopReport>(Truly([&](auto& report) {
                        EXPECT_THAT(report.failure, VariantWith<ArgumentError>(_));
                        EXPECT_THAT(report.media_index, 0);
                        EXPECT_THAT(report.culprit, Ref(media_mock_));
                        return true;
                    }))))
            .WillOnce(Return(CapacityError{}));

        EXPECT_CALL(media_mock_, pop(_)).WillOnce(Return(ArgumentError{}));

        scheduler_.runNow(+1s, [&] {
            EXPECT_THAT(transport->run(now()), UbVariantWith<AnyFailure>(VariantWith<CapacityError>(_)));
        });
    }
    // 3rd run: media #0 pop failed but transient error handler succeeded.
    {
        StrictMock<TransientErrorHandlerMock> handler_mock{};
        transport->setTransientErrorHandler(std::ref(handler_mock));
        EXPECT_CALL(handler_mock, invoke(VariantWith<MediaPopReport>(Truly([&](auto& report) {
                        EXPECT_THAT(report.failure, VariantWith<ArgumentError>(_));
                        EXPECT_THAT(report.media_index, 0);
                        EXPECT_THAT(report.culprit, Ref(media_mock_));
                        return true;
                    }))))
            .WillOnce(Return(cetl::nullopt));

        EXPECT_CALL(media_mock_, pop(_)).WillOnce(Return(ArgumentError{}));
        EXPECT_CALL(media_mock2, pop(_)).WillOnce(Return(cetl::nullopt));

        scheduler_.runNow(+1s, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    }
}

TEST_F(TestCanTransport, run_and_receive_svc_responses_with_fallible_oom_canard)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    auto transport = makeTransport(mr_mock);
    EXPECT_THAT(transport->setLocalNodeId(0x13), Eq(cetl::nullopt));

    auto maybe_session = transport->makeResponseRxSession({64, 0x17B, 0x31});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IResponseRxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IResponseRxSession>>(std::move(maybe_session));

    // Skip setFilters call; it is tested elsewhere.
    {
        EXPECT_CALL(media_mock_, pop(_)).WillOnce(Return(cetl::nullopt));
        EXPECT_CALL(media_mock_, setFilters(_)).WillOnce(Return(cetl::nullopt));

        scheduler_.runNow(+0us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    }

    // Emulate that there is constantly a new frame to receive, but the Canard RX has no memory to accept it.
    //
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly([&](auto p) {
        EXPECT_THAT(p.size(), CANARD_MTU_MAX);
        p[0] = b('0');
        p[1] = b('1');
        p[2] = b('2');
        p[3] = b(0b111'11101);
        return IMedia::PopResult::Metadata{now(), 0b111'1'0'0'101111011'0010011'0110001, 4};
    });
    EXPECT_CALL(mr_mock, do_allocate(_, _)).WillRepeatedly(Return(nullptr));

    // 1st run: canard RX has failed to accept frame and there is no transient error handler.
    {
        scheduler_.runNow(+1s, [&] {
            EXPECT_THAT(transport->run(now()), UbVariantWith<AnyFailure>(VariantWith<MemoryError>(_)));
        });
        EXPECT_THAT(session->receive(), Eq(cetl::nullopt));
    }
    // 2nd run: canard RX has failed to accept frame and there is "failing" transient error handler.
    {
        StrictMock<TransientErrorHandlerMock> handler_mock{};
        transport->setTransientErrorHandler(std::ref(handler_mock));
        EXPECT_CALL(handler_mock, invoke(_)).WillOnce(Return(StateError{}));

        scheduler_.runNow(+1s, [&] {
            EXPECT_THAT(transport->run(now()), UbVariantWith<AnyFailure>(VariantWith<StateError>(_)));
        });
        EXPECT_THAT(session->receive(), Eq(cetl::nullopt));
    }
    // 3rd run: canard RX has failed to accept frame and there is "success" transient error handler -
    // the received frame should be just dropped, but overall `run` result should be success (aka ignore OOM).
    {
        StrictMock<TransientErrorHandlerMock> handler_mock{};
        transport->setTransientErrorHandler(std::ref(handler_mock));
        EXPECT_CALL(handler_mock, invoke(_)).WillOnce(Return(cetl::nullopt));

        scheduler_.runNow(+1s, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
        EXPECT_THAT(session->receive(), Eq(cetl::nullopt));
    }
    // 4th run: fix memory problems - now we should receive the payload.
    {
        mr_mock.redirectExpectedCallsTo(mr_);

        StrictMock<TransientErrorHandlerMock> handler_mock{};
        transport->setTransientErrorHandler(std::ref(handler_mock));

        scheduler_.runNow(+1s, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
        EXPECT_THAT(session->receive(), Optional(Truly([](const auto& rx_transfer) {
                        EXPECT_THAT(rx_transfer.metadata.transfer_id, 0x1D);
                        EXPECT_THAT(rx_transfer.metadata.priority, Priority::Optional);
                        EXPECT_THAT(rx_transfer.metadata.remote_node_id, 0x31);

                        std::array<char, 3> buffer{};
                        EXPECT_THAT(rx_transfer.payload.size(), buffer.size());
                        EXPECT_THAT(rx_transfer.payload.copy(0, buffer.data(), buffer.size()), buffer.size());
                        EXPECT_THAT(buffer, ElementsAre('0', '1', '2'));

                        return true;
                    })));
    }
}

TEST_F(TestCanTransport, setLocalNodeId_when_msg_rx_subscription)
{
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));

    auto transport = makeTransport(mr_);

    auto maybe_msg_session = transport->makeMessageRxSession({0, 0x42});
    ASSERT_THAT(maybe_msg_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));

    EXPECT_CALL(media_mock_, setFilters(SizeIs(1))).WillOnce([&](Filters) {
        EXPECT_THAT(now(), TimePoint{1s});
        return cetl::nullopt;
    });
    scheduler_.runNow(+1s, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });

    ASSERT_THAT(transport->setLocalNodeId(0x13), Eq(cetl::nullopt));

    // No `setFilters` expected b/c there is no service RX subscriptions.
    scheduler_.runNow(+1s, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}

TEST_F(TestCanTransport, setLocalNodeId_when_svc_rx_subscription)
{
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));

    auto transport = makeTransport(mr_);

    auto maybe_msg_session = transport->makeMessageRxSession({0, 0x42});
    ASSERT_THAT(maybe_msg_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));

    auto maybe_svc_session = transport->makeResponseRxSession({64, 0x17B, 0x31});
    ASSERT_THAT(maybe_svc_session, VariantWith<UniquePtr<IResponseRxSession>>(NotNull()));

    EXPECT_CALL(media_mock_, setFilters(SizeIs(1))).WillOnce([&](Filters filters) {
        EXPECT_THAT(now(), TimePoint{1s});
        EXPECT_THAT(filters, Contains(FilterEq({0x4200, 0x21FFF80})));
        return cetl::nullopt;
    });
    scheduler_.runNow(+1s, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });

    ASSERT_THAT(transport->setLocalNodeId(0x13), Eq(cetl::nullopt));

    EXPECT_CALL(media_mock_, setFilters(SizeIs(2))).WillOnce([&](Filters filters) {
        EXPECT_THAT(now(), TimePoint{2s});
        EXPECT_THAT(filters, Contains(FilterEq({0x4200, 0x21FFF80})));
        EXPECT_THAT(filters, Contains(FilterEq({0x025EC980, 0x02FFFF80})));
        return cetl::nullopt;
    });
    scheduler_.runNow(+1s, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}

TEST_F(TestCanTransport, run_setFilters_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));

    auto transport = makeTransport(mr_mock);

    auto maybe_msg_session = transport->makeMessageRxSession({0, 0x42});
    ASSERT_THAT(maybe_msg_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));

    // Emulate two times that there is no memory for filters.
    //
    EXPECT_CALL(mr_mock, do_allocate(_, _)).Times(2).WillRepeatedly(Return(nullptr));
#if (__cplusplus < CETL_CPP_STANDARD_17)
    EXPECT_CALL(mr_mock, do_reallocate(nullptr, 0, _, _)).Times(2).WillRepeatedly(Return(nullptr));
#endif
    scheduler_.runNow(+1s, [&] {
        EXPECT_THAT(transport->run(now()), UbVariantWith<AnyFailure>(VariantWith<MemoryError>(_)));
    });
    scheduler_.runNow(+1s, [&] {
        EXPECT_THAT(transport->run(now()), UbVariantWith<AnyFailure>(VariantWith<MemoryError>(_)));
    });

    // Restore normal memory operation, but make media fail to accept filters.
    //
    mr_mock.redirectExpectedCallsTo(mr_);
    EXPECT_CALL(media_mock_, setFilters(SizeIs(1))).Times(2).WillRepeatedly(Return(PlatformError{MyPlatformError{13}}));
    //
    scheduler_.runNow(+1s, [&] {
        EXPECT_THAT(transport->run(now()), UbVariantWith<AnyFailure>(VariantWith<PlatformError>(Truly([](auto err) {
                        EXPECT_THAT(err->code(), 13);
                        return true;
                    }))));
    });
    scheduler_.runNow(+1s, [&] {
        EXPECT_THAT(transport->run(now()),
                    UbVariantWith<AnyFailure>(VariantWith<PlatformError>(Truly([](const auto& err) {
                        EXPECT_THAT(err->code(), 13);
                        return true;
                    }))));
    });

    // And finally, make the media accept filters - should happen once (@5s)!
    //
    EXPECT_CALL(media_mock_, setFilters(SizeIs(1))).WillOnce([&](Filters) {
        EXPECT_THAT(now(), TimePoint{5s});
        return cetl::nullopt;
    });
    scheduler_.runNow(+1s, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+1s, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}

TEST_F(TestCanTransport, run_setFilters_no_transient_handler)
{
    StrictMock<MediaMock> media_mock2{};
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));
    EXPECT_CALL(media_mock2, pop(_)).WillRepeatedly(Return(cetl::nullopt));
    EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_CAN_CLASSIC));

    auto transport = makeTransport(mr_, &media_mock2);

    auto maybe_msg_session = transport->makeMessageRxSession({0, 0x42});
    ASSERT_THAT(maybe_msg_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));

    // 1st `run`: No transient handler for setFilters,
    //            so `media_mock_.setFilters` error will be returned (and no call to `media_mock2`).
    //
    EXPECT_CALL(media_mock_, setFilters(SizeIs(1))).WillOnce(Return(CapacityError{}));
    //
    scheduler_.runNow(+1s, [&] {
        EXPECT_THAT(transport->run(now()), UbVariantWith<AnyFailure>(VariantWith<CapacityError>(_)));
    });

    // 2nd `run`: Now `media_mock_.setFilters` succeeds,
    //            and so redundant `media_mock2` will be called as well.
    //
    EXPECT_CALL(media_mock_, setFilters(SizeIs(1))).WillOnce(Return(cetl::nullopt));
    EXPECT_CALL(media_mock2, setFilters(SizeIs(1))).WillOnce(Return(cetl::nullopt));
    //
    scheduler_.runNow(+1s, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });

    // 3rd `run`: Will do nothing.
    scheduler_.runNow(+1s, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}

TEST_F(TestCanTransport, run_setFilters_with_transient_handler)
{
    using Report = ICanTransport::TransientErrorReport;

    StrictMock<MediaMock> media_mock2{};
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));
    EXPECT_CALL(media_mock2, pop(_)).WillRepeatedly(Return(cetl::nullopt));
    EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_CAN_CLASSIC));

    auto transport = makeTransport(mr_, &media_mock2);

    auto maybe_msg_session = transport->makeMessageRxSession({0, 0x42});
    ASSERT_THAT(maybe_msg_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));

    // 1st `run`: Transient handler for `setFilters` call will fail to handle the error,
    //            so the handler's result error will be returned (and no call to `media_mock2`).
    //
    transport->setTransientErrorHandler([&](Report::Variant& report_var) {
        EXPECT_THAT(report_var, VariantWith<Report::MediaConfig>(Truly([&](auto& report) {
                        EXPECT_THAT(report.failure, VariantWith<CapacityError>(_));
                        EXPECT_THAT(report.media_index, 0);
                        EXPECT_THAT(report.culprit, Ref(media_mock_));
                        return true;
                    })));
        return StateError{};
    });
    EXPECT_CALL(media_mock_, setFilters(SizeIs(1))).WillOnce(Return(CapacityError{}));
    //
    scheduler_.runNow(+1s, [&] {
        EXPECT_THAT(transport->run(now()), UbVariantWith<AnyFailure>(VariantWith<StateError>(_)));
    });

    // 2nd `run`: `media_mock_.setFilters` will fail again but now handler will handle the error,
    //            and so redundant `media_mock2` will be called as well.
    //
    transport->setTransientErrorHandler([&](Report::Variant& report_var) {
        EXPECT_THAT(report_var, VariantWith<Report::MediaConfig>(Truly([&](auto& report) {
                        EXPECT_THAT(report.failure, VariantWith<CapacityError>(_));
                        EXPECT_THAT(report.media_index, 0);
                        EXPECT_THAT(report.culprit, Ref(media_mock_));
                        return true;
                    })));
        return cetl::nullopt;
    });
    EXPECT_CALL(media_mock_, setFilters(SizeIs(1))).WillOnce(Return(CapacityError{}));
    EXPECT_CALL(media_mock2, setFilters(SizeIs(1))).WillOnce(Return(cetl::nullopt));
    //
    scheduler_.runNow(+1s, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });

    // 3rd `run`: Will still try set filters (b/c there was transient error before).
    //
    EXPECT_CALL(media_mock_, setFilters(SizeIs(1))).WillOnce(Return(cetl::nullopt));
    EXPECT_CALL(media_mock2, setFilters(SizeIs(1))).WillOnce(Return(cetl::nullopt));
    //
    scheduler_.runNow(+1s, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });

    // 4th `run`: Will do nothing.
    scheduler_.runNow(+1s, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace

namespace cetl
{

// Just random id: AD188CA2-0582-47A0-BCCF-C20E5E146213
template <>
constexpr type_id type_id_getter<MyPlatformError>() noexcept
{
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
    return {0xAD, 0x18, 0x8C, 0xA2, 0x05, 0x82, 0x47, 0xA0, 0xBC, 0xCF, 0xC2, 0x0E, 0x5E, 0x14, 0x62, 0x13};
}

}  // namespace cetl
