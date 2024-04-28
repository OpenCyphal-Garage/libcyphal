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
using testing::InSequence;
using testing::StrictMock;
using testing::ElementsAre;
using testing::VariantWith;

using namespace std::chrono_literals;

class TestCanSvcTxSessions : public testing::Test
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
                                                          const NodeId                local_node_id,
                                                          const std::size_t           tx_capacity = 16)
    {
        std::array<IMedia*, 1> media_array{&media_mock_};

        // TODO: `local_node_id` could be just passed to `can::makeTransport` as an argument,
        // but it's not possible due to CETL issue https://github.com/OpenCyphal/CETL/issues/119.
        const auto opt_local_node_id = cetl::optional<NodeId>{local_node_id};

        auto maybe_transport = can::makeTransport(mr, mux_mock_, media_array, tx_capacity, opt_local_node_id);
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));
        return cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
    }

    // MARK: Data members:

    VirtualTimeScheduler        scheduler_{};
    TrackingMemoryResource      mr_;
    StrictMock<MultiplexerMock> mux_mock_{};
    StrictMock<MediaMock>       media_mock_{};
};

// MARK: Tests:

TEST_F(TestCanSvcTxSessions, make_request_session)
{
    auto transport = makeTransport(mr_, 0);

    auto maybe_session = transport->makeRequestTxSession({123, CANARD_NODE_ID_MAX});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IRequestTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IRequestTxSession>>(std::move(maybe_session));

    EXPECT_THAT(session->getParams().service_id, 123);
    EXPECT_THAT(session->getParams().server_node_id, CANARD_NODE_ID_MAX);

    session->run(now());
}

TEST_F(TestCanSvcTxSessions, make_request_fails_due_to_argument_error)
{
    auto transport = makeTransport(mr_, 0);

    // Try invalid service id
    {
        auto maybe_session = transport->makeRequestTxSession({CANARD_SERVICE_ID_MAX + 1, 0});
        EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<ArgumentError>(_)));
    }

    // Try invalid server node id
    {
        auto maybe_session = transport->makeRequestTxSession({0, CANARD_NODE_ID_MAX + 1});
        EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<ArgumentError>(_)));
    }
}

TEST_F(TestCanSvcTxSessions, make_request_fails_due_to_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    // Emulate that there is no memory available for the message session.
    EXPECT_CALL(mr_mock, do_allocate(sizeof(can::detail::SvcRequestTxSession), _)).WillOnce(Return(nullptr));

    auto transport = makeTransport(mr_mock, CANARD_NODE_ID_MAX);

    auto maybe_session = transport->makeRequestTxSession({0x23, 0});
    EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<MemoryError>(_)));
}

TEST_F(TestCanSvcTxSessions, send_request)
{
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));

    auto transport = makeTransport(mr_, 13);

    auto maybe_session = transport->makeRequestTxSession({123, 31});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IRequestTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IRequestTxSession>>(std::move(maybe_session));

    scheduler_.runNow(+10s);
    const auto send_time = now();
    const auto timeout   = 100ms;
    session->setSendTimeout(timeout);

    const PayloadFragments empty_payload{};
    const TransferMetadata metadata{0x66, send_time, Priority::Slow};

    auto maybe_error = session->send(metadata, empty_payload);
    EXPECT_THAT(maybe_error, Eq(cetl::nullopt));

    EXPECT_CALL(media_mock_, push(_, _, _)).WillOnce([&](auto deadline, auto can_id, auto payload) {
        EXPECT_THAT(now(), send_time + 10ms);
        EXPECT_THAT(deadline, send_time + timeout);
        EXPECT_THAT(can_id, ServiceOfCanIdEq(123));
        EXPECT_THAT(can_id, AllOf(SourceNodeOfCanIdEq(13), DestinationNodeOfCanIdEq(31)));
        EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsServiceCanId()));

        auto tbm = TailByteEq(metadata.transfer_id);
        EXPECT_THAT(payload, ElementsAre(tbm));
        return true;
    });

    scheduler_.runNow(+10ms, [&] { transport->run(now()); });
    scheduler_.runNow(+10ms, [&] { transport->run(now()); });
}

TEST_F(TestCanSvcTxSessions, send_request_with_argument_error)
{
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));

    // Make initially anonymous node transport.
    //
    std::array<IMedia*, 1> media_array{&media_mock_};
    auto                   maybe_transport = can::makeTransport(mr_, mux_mock_, media_array, 2, {});
    ASSERT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));
    auto transport = cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));

    auto maybe_session = transport->makeRequestTxSession({123, 31});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IRequestTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IRequestTxSession>>(std::move(maybe_session));

    scheduler_.runNow(+100ms);
    const auto timeout       = 1s;
    const auto transfer_time = now();

    const PayloadFragments empty_payload{};
    const TransferMetadata metadata{0x66, transfer_time, Priority::Immediate};

    // Should fail due to anonymous node.
    {
        scheduler_.setNow(TimePoint{200ms});

        const auto maybe_error = session->send(metadata, empty_payload);
        EXPECT_THAT(maybe_error, Optional(VariantWith<ArgumentError>(_)));

        scheduler_.runNow(+10ms, [&] { transport->run(now()); });
        scheduler_.runNow(+10ms, [&] { transport->run(now()); });
    }

    // Fix anonymous node
    {
        scheduler_.setNow(TimePoint{300ms});
        const auto send_time = now();

        EXPECT_THAT(transport->setLocalNodeId(13), Eq(cetl::nullopt));
        const auto maybe_error = session->send(metadata, empty_payload);
        EXPECT_THAT(maybe_error, Eq(cetl::nullopt));

        EXPECT_CALL(media_mock_, push(_, _, _)).WillOnce([&](auto deadline, auto can_id, auto payload) {
            EXPECT_THAT(now(), send_time + 10ms);
            EXPECT_THAT(deadline, transfer_time + timeout);
            EXPECT_THAT(can_id, ServiceOfCanIdEq(123));
            EXPECT_THAT(can_id, AllOf(SourceNodeOfCanIdEq(13), DestinationNodeOfCanIdEq(31)));
            EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsServiceCanId()));

            auto tbm = TailByteEq(metadata.transfer_id);
            EXPECT_THAT(payload, ElementsAre(tbm));
            return true;
        });

        scheduler_.runNow(+10ms, [&] { transport->run(now()); });
        scheduler_.runNow(+10ms, [&] { transport->run(now()); });
    }
}

TEST_F(TestCanSvcTxSessions, make_response_session)
{
    auto transport = makeTransport(mr_, CANARD_NODE_ID_MAX, 2);

    auto maybe_session = transport->makeResponseTxSession({123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IResponseTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IResponseTxSession>>(std::move(maybe_session));

    EXPECT_THAT(session->getParams().service_id, 123);

    session->run(now());
}

TEST_F(TestCanSvcTxSessions, make_response_fails_due_to_argument_error)
{
    auto transport = makeTransport(mr_, 0);

    // Try invalid service id
    auto maybe_session = transport->makeResponseTxSession({CANARD_SERVICE_ID_MAX + 1});
    EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<ArgumentError>(_)));
}

TEST_F(TestCanSvcTxSessions, make_response_fails_due_to_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    // Emulate that there is no memory available for the message session.
    EXPECT_CALL(mr_mock, do_allocate(sizeof(can::detail::SvcRequestTxSession), _)).WillOnce(Return(nullptr));

    auto transport = makeTransport(mr_mock, CANARD_NODE_ID_MAX);

    auto maybe_session = transport->makeResponseTxSession({0x23});
    EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<MemoryError>(_)));
}

TEST_F(TestCanSvcTxSessions, send_respose)
{
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));

    auto transport = makeTransport(mr_, 31);

    auto maybe_session = transport->makeResponseTxSession({123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IResponseTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IResponseTxSession>>(std::move(maybe_session));

    scheduler_.runNow(+10s);
    const auto send_time = now();
    const auto timeout   = 100ms;
    session->setSendTimeout(timeout);

    const PayloadFragments        empty_payload{};
    const ServiceTransferMetadata metadata{{0x66, send_time, Priority::Fast}, 13};

    auto maybe_error = session->send(metadata, empty_payload);
    EXPECT_THAT(maybe_error, Eq(cetl::nullopt));

    EXPECT_CALL(media_mock_, push(_, _, _)).WillOnce([&](auto deadline, auto can_id, auto payload) {
        EXPECT_THAT(now(), send_time + 10ms);
        EXPECT_THAT(deadline, send_time + timeout);
        EXPECT_THAT(can_id, ServiceOfCanIdEq(123));
        EXPECT_THAT(can_id, AllOf(SourceNodeOfCanIdEq(31), DestinationNodeOfCanIdEq(13)));
        EXPECT_THAT(can_id, AllOf(PriorityOfCanIdEq(metadata.priority), IsServiceCanId()));

        auto tbm = TailByteEq(metadata.transfer_id);
        EXPECT_THAT(payload, ElementsAre(tbm));
        return true;
    });

    scheduler_.runNow(+10ms, [&] { transport->run(now()); });
    scheduler_.runNow(+10ms, [&] { transport->run(now()); });
}

TEST_F(TestCanSvcTxSessions, send_respose_with_argument_error)
{
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));

    // Make initially anonymous node transport.
    //
    std::array<IMedia*, 1> media_array{&media_mock_};
    auto                   maybe_transport = can::makeTransport(mr_, mux_mock_, media_array, 2, {});
    ASSERT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));
    auto transport = cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));

    auto maybe_session = transport->makeResponseTxSession({123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IResponseTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IResponseTxSession>>(std::move(maybe_session));

    const PayloadFragments  empty_payload{};
    ServiceTransferMetadata metadata{{0x66, now(), Priority::Immediate}, 13};

    // Should fail due to anonymous node.
    {
        scheduler_.setNow(TimePoint{100ms});

        const auto maybe_error = session->send(metadata, empty_payload);
        EXPECT_THAT(maybe_error, Optional(VariantWith<ArgumentError>(_)));

        scheduler_.runNow(+10ms, [&] { transport->run(now()); });
        scheduler_.runNow(+10ms, [&] { transport->run(now()); });
    }

    // Fix anonymous node, but break remote node id.
    {
        scheduler_.setNow(TimePoint{200ms});

        EXPECT_THAT(transport->setLocalNodeId(31), Eq(cetl::nullopt));
        metadata.remote_node_id = CANARD_NODE_ID_MAX + 1;
        const auto maybe_error  = session->send(metadata, empty_payload);
        EXPECT_THAT(maybe_error, Optional(VariantWith<ArgumentError>(_)));

        scheduler_.runNow(+10ms, [&] { transport->run(now()); });
        scheduler_.runNow(+10ms, [&] { transport->run(now()); });
    }
}

}  // namespace
