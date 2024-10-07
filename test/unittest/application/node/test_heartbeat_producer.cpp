/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "gtest_helpers.hpp"  // NOLINT(misc-include-cleaner)
#include "tracking_memory_resource.hpp"
#include "transport/msg_sessions_mock.hpp"
#include "transport/transport_gtest_helpers.hpp"
#include "transport/transport_mock.hpp"
#include "virtual_time_scheduler.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/application/node/heartbeat_producer.hpp>
#include <libcyphal/errors.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <uavcan/node/Health_1_0.hpp>
#include <uavcan/node/Heartbeat_1_0.hpp>
#include <uavcan/node/Mode_1_0.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

using libcyphal::TimePoint;
using namespace libcyphal::application;   // NOLINT This our main concern here in the unit tests.
using namespace libcyphal::presentation;  // NOLINT This our main concern here in the unit tests.
using namespace libcyphal::transport;     // NOLINT This our main concern here in the unit tests.

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::IsEmpty;
using testing::StrictMock;
using testing::ElementsAre;
using testing::VariantWith;

// https://github.com/llvm/llvm-project/issues/53444
// NOLINTBEGIN(misc-unused-using-decls, misc-include-cleaner)
using std::literals::chrono_literals::operator""s;
using std::literals::chrono_literals::operator""ms;
// NOLINTEND(misc-unused-using-decls, misc-include-cleaner)

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestHeartbeatProducer : public testing::Test
{
protected:
    using UniquePtrMsgTxSpec = MessageTxSessionMock::RefWrapper::Spec;

    void SetUp() override
    {
        cetl::pmr::set_default_resource(&mr_default_);

        EXPECT_CALL(transport_mock_, getProtocolParams())
            .WillRepeatedly(Return(ProtocolParams{std::numeric_limits<TransferId>::max(), 0, 0}));
    }

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);

        EXPECT_THAT(mr_default_.allocations, IsEmpty());
        EXPECT_THAT(mr_default_.total_allocated_bytes, mr_default_.total_deallocated_bytes);
        EXPECT_THAT(mr_default_.total_allocated_bytes, 0);
    }

    TimePoint now() const
    {
        return scheduler_.now();
    }

    // MARK: Data members:

    // NOLINTBEGIN
    libcyphal::VirtualTimeScheduler scheduler_{};
    TrackingMemoryResource          mr_;
    StrictMock<TransportMock>       transport_mock_;
    // NOLINTEND
};

// MARK: - Tests:

TEST_F(TestHeartbeatProducer, make)
{
    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<MessageTxSessionMock> msg_tx_session_mock;
    constexpr MessageTxParams        tx_params{uavcan::node::Heartbeat_1_0::_traits_::FixedPortId};
    EXPECT_CALL(msg_tx_session_mock, getParams()).WillOnce(Return(tx_params));
    EXPECT_CALL(msg_tx_session_mock, deinit()).Times(1);

    EXPECT_CALL(transport_mock_, makeMessageTxSession(MessageTxParamsEq(tx_params)))  //
        .WillOnce(Invoke([&](const auto&) {                                           //
            return libcyphal::detail::makeUniquePtr<UniquePtrMsgTxSpec>(mr_, msg_tx_session_mock);
        }));
    EXPECT_CALL(transport_mock_, getLocalNodeId())  //
        .WillRepeatedly(Return(cetl::nullopt));

    cetl::optional<node::HeartbeatProducer> heartbeat_producer;
    TimePoint                               start_time{};

    std::vector<std::tuple<TimePoint, int>> calls;

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        start_time                    = now();
        auto maybe_heartbeat_producer = node::HeartbeatProducer::make(presentation);
        ASSERT_THAT(maybe_heartbeat_producer, VariantWith<node::HeartbeatProducer>(_));
        heartbeat_producer.emplace(cetl::get<node::HeartbeatProducer>(std::move(maybe_heartbeat_producer)));
    });
    scheduler_.scheduleAt(2s + 500ms, [&](const auto&) {
        //
        EXPECT_CALL(transport_mock_, getLocalNodeId())  //
            .WillRepeatedly(Return(cetl::optional<NodeId>{NodeId{42U}}));
    });
    scheduler_.scheduleAt(3s, [&](const auto&) {
        //
        EXPECT_CALL(msg_tx_session_mock, send(TransferTxMetadataEq({{1, Priority::Nominal}, now() + 1s}), _))  //
            .WillOnce(Return(cetl::nullopt));
    });
    scheduler_.scheduleAt(3s + 500ms, [&](const auto&) {
        //
        heartbeat_producer->setUpdateCallback([&](const auto& arg) {
            //
            calls.emplace_back(arg.approx_now, arg.message.health.value);
            EXPECT_THAT(arg.approx_now, now());
            EXPECT_THAT(arg.message.uptime,
                        std::chrono::duration_cast<std::chrono::seconds>(now() - start_time).count());
            EXPECT_THAT(arg.message.health.value, uavcan::node::Health_1_0::NOMINAL);
            EXPECT_THAT(arg.message.mode.value, uavcan::node::Mode_1_0::OPERATIONAL);
        });
    });
    scheduler_.scheduleAt(4s, [&](const auto&) {
        //
        EXPECT_CALL(msg_tx_session_mock, send(TransferTxMetadataEq({{2, Priority::Nominal}, now() + 1s}), _))  //
            .WillOnce(Return(cetl::nullopt));
    });
    scheduler_.scheduleAt(5s, [&](const auto&) {
        //
        EXPECT_CALL(msg_tx_session_mock, send(TransferTxMetadataEq({{3, Priority::Nominal}, now() + 1s}), _))  //
            .WillOnce(Return(cetl::nullopt));
    });
    scheduler_.scheduleAt(5s + 500ms, [&](const auto&) {
        //
        heartbeat_producer->setUpdateCallback([&](const auto& arg) {
            //
            arg.message.health.value = uavcan::node::Health_1_0::WARNING;
            calls.emplace_back(arg.approx_now, arg.message.health.value);
        });
    });
    scheduler_.scheduleAt(6s, [&](const auto&) {
        //
        EXPECT_CALL(msg_tx_session_mock, send(TransferTxMetadataEq({{4, Priority::Nominal}, now() + 1s}), _))  //
            .WillOnce(Return(cetl::nullopt));
    });
    scheduler_.scheduleAt(6s + 500ms, [&](const auto&) {
        //
        heartbeat_producer->setUpdateCallback([&](const auto& arg) {
            //
            calls.emplace_back(arg.approx_now, arg.message.health.value);
        });
    });
    scheduler_.scheduleAt(7s, [&](const auto&) {
        //
        EXPECT_CALL(msg_tx_session_mock, send(TransferTxMetadataEq({{5, Priority::Nominal}, now() + 1s}), _))  //
            .WillOnce(Return(cetl::nullopt));
    });
    scheduler_.scheduleAt(8s, [&](const auto&) {
        //
        EXPECT_CALL(msg_tx_session_mock, send(TransferTxMetadataEq({{6, Priority::Nominal}, now() + 1s}), _))  //
            .WillOnce(Return(cetl::nullopt));

        heartbeat_producer->message().health.value = uavcan::node::Health_1_0::CAUTION;
    });
    scheduler_.scheduleAt(8s + 500ms, [&](const auto&) {
        //
        heartbeat_producer.reset();
    });
    scheduler_.spinFor(10s);

    EXPECT_THAT(calls,
                ElementsAre(  //
                    std::make_tuple(TimePoint{4s}, static_cast<int>(uavcan::node::Health_1_0::NOMINAL)),
                    std::make_tuple(TimePoint{5s}, static_cast<int>(uavcan::node::Health_1_0::NOMINAL)),
                    std::make_tuple(TimePoint{6s}, static_cast<int>(uavcan::node::Health_1_0::WARNING)),
                    std::make_tuple(TimePoint{7s}, static_cast<int>(uavcan::node::Health_1_0::WARNING)),
                    std::make_tuple(TimePoint{8s}, static_cast<int>(uavcan::node::Health_1_0::CAUTION))));
}

TEST_F(TestHeartbeatProducer, make_failure)
{
    Presentation presentation{mr_, scheduler_, transport_mock_};

    EXPECT_CALL(transport_mock_, makeMessageTxSession(_))  //
        .WillOnce(Return(libcyphal::ArgumentError{}));

    EXPECT_THAT(node::HeartbeatProducer::make(presentation),
                VariantWith<Presentation::MakeFailure>(VariantWith<libcyphal::ArgumentError>(_)));
}

TEST_F(TestHeartbeatProducer, move)
{
    static_assert(std::is_move_constructible<node::HeartbeatProducer>::value, "Should be move constructible.");
    static_assert(!std::is_copy_assignable<node::HeartbeatProducer>::value, "Should not be copy assignable.");
    static_assert(!std::is_move_assignable<node::HeartbeatProducer>::value, "Should not be move assignable.");
    static_assert(!std::is_copy_constructible<node::HeartbeatProducer>::value, "Should not be copy constructible.");
    static_assert(!std::is_default_constructible<node::HeartbeatProducer>::value,
                  "Should not be default constructible.");

    Presentation presentation{mr_, scheduler_, transport_mock_};

    StrictMock<MessageTxSessionMock> msg_tx_session_mock;
    EXPECT_CALL(msg_tx_session_mock, getParams())  //
        .WillOnce(Return(MessageTxParams{uavcan::node::Heartbeat_1_0::_traits_::FixedPortId}));
    EXPECT_CALL(msg_tx_session_mock, send(_, _))  //
        .Times(4)
        .WillRepeatedly(Return(cetl::nullopt));
    EXPECT_CALL(msg_tx_session_mock, deinit()).Times(1);

    EXPECT_CALL(transport_mock_, makeMessageTxSession(_))  //
        .WillOnce(Invoke([&](const auto&) {                //
            return libcyphal::detail::makeUniquePtr<UniquePtrMsgTxSpec>(mr_, msg_tx_session_mock);
        }));
    EXPECT_CALL(transport_mock_, getLocalNodeId())  //
        .WillRepeatedly(Return(cetl::optional<NodeId>{NodeId{42U}}));

    std::vector<TimePoint>                  calls;
    cetl::optional<node::HeartbeatProducer> heartbeat_producer1;
    cetl::optional<node::HeartbeatProducer> heartbeat_producer2;

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        auto maybe_heartbeat = node::HeartbeatProducer::make(presentation);
        ASSERT_THAT(maybe_heartbeat, VariantWith<node::HeartbeatProducer>(_));
        heartbeat_producer1.emplace(cetl::get<node::HeartbeatProducer>(std::move(maybe_heartbeat)));
        heartbeat_producer1->setUpdateCallback([&](const auto& arg) {
            //
            calls.push_back(arg.approx_now);
        });
    });
    scheduler_.scheduleAt(2s + 500ms, [&](const auto&) {
        //
        heartbeat_producer2.emplace(std::move(*heartbeat_producer1));
    });
    scheduler_.spinFor(5s);

    EXPECT_THAT(calls, ElementsAre(TimePoint{1s}, TimePoint{2s}, TimePoint{3s}, TimePoint{4s}));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
