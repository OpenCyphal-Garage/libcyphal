/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../../cetl_gtest_helpers.hpp"
#include "../../memory_resource_mock.hpp"
#include "../../tracking_memory_resource.hpp"
#include "../../virtual_time_scheduler.hpp"
#include "can_gtest_helpers.hpp"
#include "media_mock.hpp"

#include <canard.h>
#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/can/can_transport.hpp>
#include <libcyphal/transport/can/can_transport_impl.hpp>
#include <libcyphal/transport/can/media.hpp>
#include <libcyphal/transport/can/svc_tx_sessions.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/svc_sessions.hpp>
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

using testing::_;
using testing::Eq;
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
// NOLINTEND(misc-unused-using-decls, misc-include-cleaner)

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

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
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    TimePoint now() const
    {
        return scheduler_.now();
    }

    UniquePtr<ICanTransport> makeTransport(cetl::pmr::memory_resource& mr, const NodeId local_node_id)
    {
        std::array<IMedia*, 1> media_array{&media_mock_};

        auto maybe_transport = can::makeTransport(mr, media_array, 16);
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));
        auto transport = cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));

        transport->setLocalNodeId(local_node_id);

        return transport;
    }

    // MARK: Data members:

    // NOLINTBEGIN
    libcyphal::VirtualTimeScheduler scheduler_{};
    TrackingMemoryResource          mr_;
    StrictMock<MediaMock>           media_mock_{};
    // NOLINTEND
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

    EXPECT_THAT(session->run(now()), UbVariantWithoutValue());
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

    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}

TEST_F(TestCanSvcTxSessions, send_request_with_argument_error)
{
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));

    // Make initially anonymous node transport.
    //
    std::array<IMedia*, 1> media_array{&media_mock_};
    auto                   maybe_transport = can::makeTransport(mr_, media_array, 2);
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

        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
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

        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    }
}

TEST_F(TestCanSvcTxSessions, make_response_session)
{
    auto transport = makeTransport(mr_, CANARD_NODE_ID_MAX);

    auto maybe_session = transport->makeResponseTxSession({123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IResponseTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IResponseTxSession>>(std::move(maybe_session));

    EXPECT_THAT(session->getParams().service_id, 123);

    EXPECT_THAT(session->run(now()), UbVariantWithoutValue());
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
    const ServiceTransferMetadata metadata{0x66, send_time, Priority::Fast, 13};

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

    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}

TEST_F(TestCanSvcTxSessions, send_respose_with_argument_error)
{
    EXPECT_CALL(media_mock_, pop(_)).WillRepeatedly(Return(cetl::nullopt));

    // Make initially anonymous node transport.
    //
    std::array<IMedia*, 1> media_array{&media_mock_};
    auto                   maybe_transport = can::makeTransport(mr_, media_array, 2);
    ASSERT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));
    auto transport = cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));

    auto maybe_session = transport->makeResponseTxSession({123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IResponseTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IResponseTxSession>>(std::move(maybe_session));

    const PayloadFragments  empty_payload{};
    ServiceTransferMetadata metadata{0x66, now(), Priority::Immediate, 13};

    // Should fail due to anonymous node.
    {
        scheduler_.setNow(TimePoint{100ms});

        const auto maybe_error = session->send(metadata, empty_payload);
        EXPECT_THAT(maybe_error, Optional(VariantWith<ArgumentError>(_)));

        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    }

    // Fix anonymous node, but break remote node id.
    {
        scheduler_.setNow(TimePoint{200ms});

        EXPECT_THAT(transport->setLocalNodeId(31), Eq(cetl::nullopt));
        metadata.remote_node_id = CANARD_NODE_ID_MAX + 1;
        const auto maybe_error  = session->send(metadata, empty_payload);
        EXPECT_THAT(maybe_error, Optional(VariantWith<ArgumentError>(_)));

        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    }
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
