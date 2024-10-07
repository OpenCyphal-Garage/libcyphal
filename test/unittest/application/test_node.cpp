/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "gtest_helpers.hpp"  // NOLINT(misc-include-cleaner)
#include "tracking_memory_resource.hpp"
#include "transport/msg_sessions_mock.hpp"
#include "transport/svc_sessions_mock.hpp"
#include "transport/transport_gtest_helpers.hpp"  // NOLINT(misc-include-cleaner)
#include "transport/transport_mock.hpp"
#include "virtual_time_scheduler.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/application/node.hpp>
#include <libcyphal/errors.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <uavcan/node/Health_1_0.hpp>
#include <uavcan/node/Heartbeat_1_0.hpp>

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

class TestNode : public testing::Test
{
protected:
    using UniquePtrMsgTxSpec = MessageTxSessionMock::RefWrapper::Spec;
    using UniquePtrReqRxSpec = RequestRxSessionMock::RefWrapper::Spec;
    using UniquePtrResTxSpec = ResponseTxSessionMock::RefWrapper::Spec;

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

    void setupDefaultExpectations()
    {
        EXPECT_CALL(getinfo_req_rx_session_mock_, setOnReceiveCallback(_)).WillRepeatedly(Return());
        EXPECT_CALL(getinfo_req_rx_session_mock_, deinit()).Times(1);

        EXPECT_CALL(getinfo_res_tx_session_mock_, deinit()).Times(1);

        constexpr MessageTxParams tx_params{uavcan::node::Heartbeat_1_0::_traits_::FixedPortId};
        EXPECT_CALL(heartbeat_msg_tx_session_mock_, getParams()).WillOnce(Return(tx_params));
        EXPECT_CALL(heartbeat_msg_tx_session_mock_, deinit()).Times(1);

        EXPECT_CALL(transport_mock_, makeRequestRxSession(_))  //
            .WillOnce(Invoke([&](const auto&) {                //
                return libcyphal::detail::makeUniquePtr<UniquePtrReqRxSpec>(mr_, getinfo_req_rx_session_mock_);
            }));
        EXPECT_CALL(transport_mock_, makeResponseTxSession(_))  //
            .WillOnce(Invoke([&](const auto&) {                 //
                return libcyphal::detail::makeUniquePtr<UniquePtrResTxSpec>(mr_, getinfo_res_tx_session_mock_);
            }));
        EXPECT_CALL(transport_mock_, makeMessageTxSession(_))  //
            .WillOnce(Invoke([&](const auto&) {                //
                return libcyphal::detail::makeUniquePtr<UniquePtrMsgTxSpec>(mr_, heartbeat_msg_tx_session_mock_);
            }));
    }

    // MARK: Data members:

    // NOLINTBEGIN
    libcyphal::VirtualTimeScheduler   scheduler_{};
    TrackingMemoryResource            mr_;
    StrictMock<TransportMock>         transport_mock_;
    StrictMock<RequestRxSessionMock>  getinfo_req_rx_session_mock_;
    StrictMock<ResponseTxSessionMock> getinfo_res_tx_session_mock_;
    StrictMock<MessageTxSessionMock>  heartbeat_msg_tx_session_mock_;
    // NOLINTEND
};

// MARK: - Tests:

TEST_F(TestNode, make)
{
    setupDefaultExpectations();

    EXPECT_CALL(transport_mock_, getLocalNodeId())  //
        .WillRepeatedly(Return(cetl::optional<NodeId>{NodeId{42U}}));

    cetl::optional<Node>                    node;
    std::vector<std::tuple<TimePoint, int>> calls;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        auto maybe_node = Node::make(presentation);
        ASSERT_THAT(maybe_node, VariantWith<Node>(_));
        node.emplace(cetl::get<Node>(std::move(maybe_node)));
        node->heartbeatProducer().setUpdateCallback([&](const auto& arg) {
            //
            calls.emplace_back(arg.approx_now, arg.message.health.value);
        });

        EXPECT_CALL(heartbeat_msg_tx_session_mock_, send(_, _))  //
            .WillRepeatedly(Return(cetl::nullopt));
    });
    scheduler_.scheduleAt(3s + 500ms, [&](const auto&) {
        //
        node.reset();
    });
    scheduler_.spinFor(10s);

    EXPECT_THAT(calls,
                ElementsAre(  //
                    std::make_tuple(TimePoint{1s}, static_cast<int>(uavcan::node::Health_1_0::NOMINAL)),
                    std::make_tuple(TimePoint{2s}, static_cast<int>(uavcan::node::Health_1_0::NOMINAL)),
                    std::make_tuple(TimePoint{3s}, static_cast<int>(uavcan::node::Health_1_0::NOMINAL))));
}

TEST_F(TestNode, make_failures)
{
    Presentation presentation{mr_, scheduler_, transport_mock_};

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        EXPECT_CALL(transport_mock_, makeMessageTxSession(_))  //
            .WillOnce(Return(libcyphal::ArgumentError{}));

        EXPECT_THAT(Node::make(presentation), VariantWith<Node::MakeFailure>(VariantWith<libcyphal::ArgumentError>(_)));
    });
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        constexpr MessageTxParams tx_params{uavcan::node::Heartbeat_1_0::_traits_::FixedPortId};
        EXPECT_CALL(heartbeat_msg_tx_session_mock_, getParams()).WillOnce(Return(tx_params));
        EXPECT_CALL(heartbeat_msg_tx_session_mock_, deinit()).Times(1);

        EXPECT_CALL(transport_mock_, makeMessageTxSession(_))  //
            .WillOnce(Invoke([&](const auto&) {                //
                return libcyphal::detail::makeUniquePtr<UniquePtrMsgTxSpec>(mr_, heartbeat_msg_tx_session_mock_);
            }));
        EXPECT_CALL(transport_mock_, makeRequestRxSession(_))  //
            .WillOnce(Return(libcyphal::ArgumentError{}));

        EXPECT_THAT(Node::make(presentation), VariantWith<Node::MakeFailure>(VariantWith<libcyphal::ArgumentError>(_)));
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestNode, move)
{
    static_assert(std::is_move_constructible<Node>::value, "Should be move constructible.");
    static_assert(!std::is_copy_assignable<Node>::value, "Should not be copy assignable.");
    static_assert(!std::is_move_assignable<Node>::value, "Should not be move assignable.");
    static_assert(!std::is_copy_constructible<Node>::value, "Should not be copy constructible.");
    static_assert(!std::is_default_constructible<Node>::value, "Should not be default constructible.");

    setupDefaultExpectations();

    EXPECT_CALL(heartbeat_msg_tx_session_mock_, send(_, _))  //
        .Times(4)
        .WillRepeatedly(Return(cetl::nullopt));

    EXPECT_CALL(transport_mock_, getLocalNodeId())  //
        .WillRepeatedly(Return(cetl::optional<NodeId>{NodeId{42U}}));

    cetl::optional<Node>   node1;
    cetl::optional<Node>   node2;
    std::vector<TimePoint> calls;

    Presentation presentation{mr_, scheduler_, transport_mock_};

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        auto maybe_node = Node::make(presentation);
        ASSERT_THAT(maybe_node, VariantWith<Node>(_));
        node1.emplace(cetl::get<Node>(std::move(maybe_node)));
        node1->heartbeatProducer().setUpdateCallback([&](const auto& arg) {
            //
            calls.emplace_back(arg.approx_now);
        });
    });
    scheduler_.scheduleAt(2s + 500ms, [&](const auto&) {
        //
        node2.emplace(std::move(*node1));
    });
    scheduler_.spinFor(5s);

    EXPECT_THAT(calls, ElementsAre(TimePoint{1s}, TimePoint{2s}, TimePoint{3s}, TimePoint{4s}));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
