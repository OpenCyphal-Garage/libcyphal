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

#include <limits>
#include <gmock/gmock.h>

namespace
{
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
        // TODO: Uncomment this when PMR deleter is fixed.
        // EXPECT_EQ(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    TimePoint now() const
    {
        return scheduler_.now();
    }

    CETL_NODISCARD UniquePtr<ICanTransport> makeTransport(cetl::pmr::memory_resource& mr,
                                                          IMedia*                     extra_media = nullptr,
                                                          const std::size_t           tx_capacity = 16)
    {
        std::array<IMedia*, 2> media_array{&media_mock_, extra_media};

        auto maybe_transport = can::makeTransport(mr, mux_mock_, media_array, tx_capacity, {});
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

TEST_F(TestCanTransport, makeTransport_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    // Emulate that there is no memory available for the transport.
    EXPECT_CALL(mr_mock, do_allocate(sizeof(can::detail::TransportImpl), _)).WillOnce(Return(nullptr));

    std::array<IMedia*, 1> media_array{&media_mock_};
    auto                   maybe_transport = can::makeTransport(mr_mock, mux_mock_, media_array, 0, {});
    EXPECT_THAT(maybe_transport, VariantWith<FactoryError>(VariantWith<MemoryError>(_)));
}

TEST_F(TestCanTransport, makeTransport_too_many_media)
{
    // Canard use `std::uint8_t` as a media index, so 256+ media interfaces are not allowed.
    std::array<IMedia*, std::numeric_limits<std::uint8_t>::max() + 1> media_array{};
    std::fill(media_array.begin(), media_array.end(), &media_mock_);

    auto maybe_transport = can::makeTransport(mr_, mux_mock_, media_array, 0, {});
    EXPECT_THAT(maybe_transport, VariantWith<FactoryError>(VariantWith<ArgumentError>(_)));
}

TEST_F(TestCanTransport, makeTransport_getLocalNodeId)
{
    // Anonymous node
    {
        std::array<IMedia*, 1> media_array{&media_mock_};
        auto                   maybe_transport = can::makeTransport(mr_, mux_mock_, media_array, 0, {});
        ASSERT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));

        auto transport = cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
        EXPECT_THAT(transport->getLocalNodeId(), Eq(cetl::nullopt));
    }

    // Node with ID
    {
        const auto node_id = cetl::make_optional(static_cast<NodeId>(42));

        std::array<IMedia*, 1> media_array{&media_mock_};
        auto                   maybe_transport = can::makeTransport(mr_, mux_mock_, media_array, 0, node_id);
        ASSERT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));

        auto transport = cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
        EXPECT_THAT(transport->getLocalNodeId(), Optional(42));
    }

    // Two media interfaces
    {
        StrictMock<MediaMock> media_mock2;
        EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_MAX));

        std::array<IMedia*, 3> media_array{&media_mock_, nullptr, &media_mock2};
        auto                   maybe_transport = can::makeTransport(mr_, mux_mock_, media_array, 0, {});
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));
    }

    // All 3 maximum number of media interfaces
    {
        StrictMock<MediaMock> media_mock2{}, media_mock3{};
        EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_MAX));
        EXPECT_CALL(media_mock3, getMtu()).WillRepeatedly(Return(CANARD_MTU_MAX));

        std::array<IMedia*, 3> media_array{&media_mock_, &media_mock2, &media_mock3};
        auto                   maybe_transport = can::makeTransport(mr_, mux_mock_, media_array, 0, {});
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));
    }
}

TEST_F(TestCanTransport, setLocalNodeId)
{
    auto transport = makeTransport(mr_);

    EXPECT_THAT(transport->setLocalNodeId(CANARD_NODE_ID_MAX + 1), Optional(testing::A<ArgumentError>()));
    EXPECT_THAT(transport->getLocalNodeId(), Eq(cetl::nullopt));

    EXPECT_THAT(transport->setLocalNodeId(CANARD_NODE_ID_MAX), Eq(cetl::nullopt));
    EXPECT_THAT(transport->getLocalNodeId(), Optional(CANARD_NODE_ID_MAX));

    EXPECT_THAT(transport->setLocalNodeId(CANARD_NODE_ID_MAX), Eq(cetl::nullopt));
    EXPECT_THAT(transport->getLocalNodeId(), Optional(CANARD_NODE_ID_MAX));

    EXPECT_THAT(transport->setLocalNodeId(0), Optional(testing::A<ArgumentError>()));
    EXPECT_THAT(transport->getLocalNodeId(), Optional(CANARD_NODE_ID_MAX));
}

TEST_F(TestCanTransport, makeTransport_with_invalid_arguments)
{
    // No media
    {
        const auto node_id = cetl::make_optional(static_cast<NodeId>(CANARD_NODE_ID_MAX));

        const auto maybe_transport = can::makeTransport(mr_, mux_mock_, {}, 0, node_id);
        EXPECT_THAT(maybe_transport, VariantWith<FactoryError>(VariantWith<ArgumentError>(_)));
    }

    // try just a bit bigger than max canard id (aka 128)
    {
        const auto node_id = cetl::make_optional(static_cast<NodeId>(CANARD_NODE_ID_MAX + 1));

        std::array<IMedia*, 1> media_array{&media_mock_};
        const auto             maybe_transport = can::makeTransport(mr_, mux_mock_, media_array, 0, node_id);
        EXPECT_THAT(maybe_transport, VariantWith<FactoryError>(VariantWith<ArgumentError>(_)));
    }

    // magic 255 number (aka CANARD_NODE_ID_UNSET) can't be used as well
    {
        const auto node_id = cetl::make_optional(static_cast<NodeId>(CANARD_NODE_ID_UNSET));

        std::array<IMedia*, 1> media_array{&media_mock_};
        const auto             maybe_transport = can::makeTransport(mr_, mux_mock_, media_array, 0, node_id);
        EXPECT_THAT(maybe_transport, VariantWith<FactoryError>(VariantWith<ArgumentError>(_)));
    }

    // just in case try 0x100 (aka overflow)
    {
        const NodeId too_big = static_cast<NodeId>(std::numeric_limits<CanardNodeID>::max()) + 1;
        const auto   node_id = cetl::make_optional(too_big);

        std::array<IMedia*, 1> media_array{&media_mock_};
        const auto             maybe_transport = can::makeTransport(mr_, mux_mock_, media_array, 0, node_id);
        EXPECT_THAT(maybe_transport, VariantWith<FactoryError>(VariantWith<ArgumentError>(_)));
    }
}

TEST_F(TestCanTransport, getProtocolParams)
{
    StrictMock<MediaMock> media_mock2{};
    EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_MAX));

    std::array<IMedia*, 2> media_array{&media_mock_, &media_mock2};
    auto transport = cetl::get<UniquePtr<ICanTransport>>(can::makeTransport(mr_, mux_mock_, media_array, 0, {}));

    EXPECT_CALL(media_mock_, getMtu()).WillRepeatedly(testing::Return(CANARD_MTU_CAN_FD));
    EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(testing::Return(CANARD_MTU_CAN_CLASSIC));

    auto params = transport->getProtocolParams();
    EXPECT_THAT(params.transfer_id_modulo, 1 << CANARD_TRANSFER_ID_BIT_LENGTH);
    EXPECT_THAT(params.max_nodes, CANARD_NODE_ID_MAX + 1);
    EXPECT_THAT(params.mtu_bytes, CANARD_MTU_CAN_CLASSIC);

    // Manipulate MTU values on fly
    {
        EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(testing::Return(CANARD_MTU_CAN_FD));
        EXPECT_THAT(transport->getProtocolParams().mtu_bytes, CANARD_MTU_CAN_FD);

        EXPECT_CALL(media_mock_, getMtu()).WillRepeatedly(testing::Return(CANARD_MTU_CAN_CLASSIC));
        EXPECT_THAT(transport->getProtocolParams().mtu_bytes, CANARD_MTU_CAN_CLASSIC);

        EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(testing::Return(CANARD_MTU_CAN_CLASSIC));
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
    EXPECT_THAT(maybe_rx_session, VariantWith<AnyError>(VariantWith<ArgumentError>(_)));
}

TEST_F(TestCanTransport, makeMessageRxSession_invalid_resubscription)
{
    auto transport = makeTransport(mr_);

    const PortId test_subject_id = 111;

    auto maybe_rx_session1 = transport->makeMessageRxSession({0, test_subject_id});
    ASSERT_THAT(maybe_rx_session1, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));

    auto maybe_rx_session2 = transport->makeMessageRxSession({0, test_subject_id});
    EXPECT_THAT(maybe_rx_session2, VariantWith<AnyError>(VariantWith<AlreadyExistsError>(_)));
}

TEST_F(TestCanTransport, makeRequestRxSession_invalid_resubscription)
{
    auto transport = makeTransport(mr_);

    const PortId test_subject_id = 111;

    auto maybe_rx_session1 = transport->makeRequestRxSession({0, test_subject_id});
    ASSERT_THAT(maybe_rx_session1, VariantWith<UniquePtr<IRequestRxSession>>(NotNull()));

    auto maybe_rx_session2 = transport->makeRequestRxSession({0, test_subject_id});
    EXPECT_THAT(maybe_rx_session2, VariantWith<AnyError>(VariantWith<AlreadyExistsError>(_)));
}

TEST_F(TestCanTransport, makeResponseRxSession_invalid_resubscription)
{
    auto transport = makeTransport(mr_);

    const PortId test_subject_id = 111;

    auto maybe_rx_session1 = transport->makeResponseRxSession({0, test_subject_id, 0x31});
    ASSERT_THAT(maybe_rx_session1, VariantWith<UniquePtr<IResponseRxSession>>(NotNull()));

    auto maybe_rx_session2 = transport->makeResponseRxSession({0, test_subject_id, 0x31});
    EXPECT_THAT(maybe_rx_session2, VariantWith<AnyError>(VariantWith<AlreadyExistsError>(_)));
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

    const auto             payload = makeIotaArray<CANARD_MTU_CAN_CLASSIC>('0');
    const TransferMetadata metadata{0x13, send_time, Priority::Nominal};

    auto maybe_error = session->send(metadata, makeSpansFrom(payload));
    EXPECT_THAT(maybe_error, Optional(VariantWith<ArgumentError>(_)));

    scheduler_.runNow(+10us, [&] { transport->run(now()); });
    scheduler_.runNow(10us, [&] { session->run(now()); });
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

    const auto             payload = makeIotaArray<CANARD_MTU_CAN_CLASSIC>('0');
    const TransferMetadata metadata{0x13, send_time, Priority::Nominal};

    auto maybe_error = session->send(metadata, makeSpansFrom(payload));
    EXPECT_THAT(maybe_error, Eq(cetl::nullopt));

    {
        InSequence s;

        EXPECT_CALL(media_mock_, push(_, _, _)).WillOnce([&](auto deadline, auto can_id, auto payload) {
            EXPECT_THAT(now(), send_time + 10us);
            EXPECT_THAT(deadline, send_time + timeout);
            EXPECT_THAT(can_id, AllOf(SubjectOfCanIdEq(7), SourceNodeOfCanIdEq(0x45)));
            EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsMessageCanId()));

            auto tbm = TailByteEq(metadata.transfer_id, true, false);
            EXPECT_THAT(payload, ElementsAre(b('0'), b('1'), b('2'), b('3'), b('4'), b('5'), b('6'), tbm));
            return true;
        });
        EXPECT_CALL(media_mock_, push(_, _, _)).WillOnce([&](auto deadline, auto can_id, auto payload) {
            EXPECT_THAT(now(), send_time + 10us);
            EXPECT_THAT(deadline, send_time + timeout);
            EXPECT_THAT(can_id, AllOf(SubjectOfCanIdEq(7), SourceNodeOfCanIdEq(0x45)));
            EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsMessageCanId()));

            auto tbm = TailByteEq(metadata.transfer_id, false, true, false);
            EXPECT_THAT(payload, ElementsAre(b('7'), _, _ /* CRC bytes */, tbm));
            return true;
        });
    }

    scheduler_.runNow(+10us, [&] { transport->run(now()); });
    scheduler_.runNow(+10us, [&] { transport->run(now()); });
}

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

    const auto             payload = makeIotaArray<10>('0');
    const TransferMetadata metadata{0x13, send_time, Priority::Nominal};

    auto maybe_error = session->send(metadata, makeSpansFrom(payload));
    EXPECT_THAT(maybe_error, Eq(cetl::nullopt));

    {
        InSequence s;

        auto expectMediaCalls = [&](MediaMock& media_mock, const std::string& ctx, TimePoint when) {
            EXPECT_CALL(media_mock, push(_, _, _)).WillOnce([&, ctx, when](auto deadline, auto can_id, auto payload) {
                EXPECT_THAT(now(), when) << ctx;
                EXPECT_THAT(deadline, send_time + timeout) << ctx;
                EXPECT_THAT(can_id, AllOf(SubjectOfCanIdEq(7), SourceNodeOfCanIdEq(0x45))) << ctx;
                EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsMessageCanId())) << ctx;

                auto tbm = TailByteEq(metadata.transfer_id, true, false);
                EXPECT_THAT(payload, ElementsAre(b('0'), b('1'), b('2'), b('3'), b('4'), b('5'), b('6'), tbm)) << ctx;
                return true;
            });
            EXPECT_CALL(media_mock, push(_, _, _)).WillOnce([&, ctx, when](auto deadline, auto can_id, auto payload) {
                EXPECT_THAT(now(), when) << ctx;
                EXPECT_THAT(deadline, send_time + timeout) << ctx;
                EXPECT_THAT(can_id, AllOf(SubjectOfCanIdEq(7), SourceNodeOfCanIdEq(0x45))) << ctx;
                EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsMessageCanId())) << ctx;

                auto tbm = TailByteEq(metadata.transfer_id, false, true, false);
                EXPECT_THAT(payload, ElementsAre(b('7'), b('8'), b('9'), b(0x7D), b(0x61) /* CRC bytes */, tbm)) << ctx;
                return true;
            });
        };

        // Emulate once that the first media is not ready to push fragment (@10us). So transport will
        // switch to the second media, and only on the next run will (@20us) retry with the first media again.
        //
        EXPECT_CALL(media_mock_, push(_, _, _)).WillOnce([&](auto, auto, auto) {
            EXPECT_THAT(now(), send_time + 10us);
            return false;
        });
        expectMediaCalls(media_mock2, "M#2", send_time + 10us);
        expectMediaCalls(media_mock_, "M#1", send_time + 20us);
    }

    scheduler_.runNow(+10us, [&] { transport->run(now()); });
    scheduler_.runNow(+10us, [&] { transport->run(now()); });
    scheduler_.runNow(+10us, [&] { transport->run(now()); });
}

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

    const auto epoch = TimePoint{10s};
    scheduler_.setNow(epoch);
    const auto rx1_timestamp = epoch;
    const auto rx2_timestamp = epoch + 2 * timeout;
    {
        InSequence seq;

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
            return RxMetadata{rx1_timestamp, 0b111'1'0'0'101111011'0010011'0110001, 8};
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
            return RxMetadata{rx2_timestamp, 0b111'1'0'0'101111011'0010011'0110001, 8};
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
            return RxMetadata{rx2_timestamp + 1ms, 0b111'1'0'0'101111011'0010011'0110001, 6};
        });
    }
    scheduler_.runNow(+10ms, [&] { transport->run(now()); });
    scheduler_.runNow(+10ms, [&] { session->run(now()); });
    scheduler_.setNow(rx2_timestamp);
    scheduler_.runNow(+10ms, [&] { transport->run(now()); });
    scheduler_.runNow(+10ms, [&] { session->run(now()); });
    scheduler_.runNow(+10ms, [&] { transport->run(now()); });
    scheduler_.runNow(+10ms, [&] { session->run(now()); });

    const auto maybe_rx_transfer = session->receive();
    ASSERT_THAT(maybe_rx_transfer, Optional(_));
    const auto& rx_transfer = maybe_rx_transfer.value();

    EXPECT_THAT(rx_transfer.metadata.timestamp, rx2_timestamp);
    EXPECT_THAT(rx_transfer.metadata.transfer_id, 0x1E);
    EXPECT_THAT(rx_transfer.metadata.priority, Priority::Optional);
    EXPECT_THAT(rx_transfer.metadata.remote_node_id, 0x31);

    std::array<char, 10> buffer{};
    EXPECT_THAT(rx_transfer.payload.size(), buffer.size());
    EXPECT_THAT(rx_transfer.payload.copy(0, buffer.data(), buffer.size()), buffer.size());
    EXPECT_THAT(buffer, ElementsAre('0', '1', '2', '3', '4', '5', '6', '7', '8', '9'));
}

}  // namespace
