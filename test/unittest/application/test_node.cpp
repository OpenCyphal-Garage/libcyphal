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
#include <libcyphal/application/node.hpp>
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

class TestNode : public testing::Test
{
protected:
    using UniquePtrMsgTxSpec = MessageTxSessionMock::RefWrapper::Spec;

    void SetUp() override
    {
        EXPECT_CALL(transport_mock_, getProtocolParams())
            .WillRepeatedly(Return(ProtocolParams{std::numeric_limits<TransferId>::max(), 0, 0}));
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

    // MARK: Data members:

    // NOLINTBEGIN
    libcyphal::VirtualTimeScheduler scheduler_{};
    TrackingMemoryResource          mr_;
    StrictMock<TransportMock>       transport_mock_;
    // NOLINTEND
};

// MARK: - Tests:

TEST_F(TestNode, heartbeat)
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

    cetl::optional<Node> node;
    TimePoint            start_time{};

    std::vector<std::tuple<TimePoint, int>> calls;

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        start_time      = now();
        auto maybe_node = Node::make(presentation);
        ASSERT_THAT(maybe_node, VariantWith<Node>(_));
        node.emplace(cetl::get<Node>(std::move(maybe_node)));
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
        node->getHeartbeat().setUpdateCallback([&](const auto& arg) {
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
        node->getHeartbeat().setUpdateCallback([&](const auto& arg) {
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
        node->getHeartbeat().setUpdateCallback([&](const auto& arg) {
            //
            calls.emplace_back(arg.approx_now, arg.message.health.value);
        });
    });
    scheduler_.scheduleAt(7s, [&](const auto&) {
        //
        EXPECT_CALL(msg_tx_session_mock, send(TransferTxMetadataEq({{5, Priority::Nominal}, now() + 1s}), _))  //
            .WillOnce(Return(cetl::nullopt));
    });
    scheduler_.scheduleAt(7s + 500ms, [&](const auto&) {
        //
        node.reset();
    });
    scheduler_.spinFor(10s);

    EXPECT_THAT(calls,
                ElementsAre(  //
                    std::make_tuple(TimePoint{4s}, static_cast<int>(uavcan::node::Health_1_0::NOMINAL)),
                    std::make_tuple(TimePoint{5s}, static_cast<int>(uavcan::node::Health_1_0::NOMINAL)),
                    std::make_tuple(TimePoint{6s}, static_cast<int>(uavcan::node::Health_1_0::WARNING)),
                    std::make_tuple(TimePoint{7s}, static_cast<int>(uavcan::node::Health_1_0::WARNING))));
}

TEST_F(TestNode, heartbeat_failure)
{
    Presentation presentation{mr_, scheduler_, transport_mock_};

    EXPECT_CALL(transport_mock_, makeMessageTxSession(_))  //
        .WillOnce(Return(libcyphal::ArgumentError{}));

    EXPECT_THAT(Node::make(presentation), VariantWith<Node::MakeFailure>(VariantWith<libcyphal::ArgumentError>(_)));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
