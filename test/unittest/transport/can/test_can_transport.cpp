/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <libcyphal/transport/can/transport.hpp>

#include "media_mock.hpp"
#include "../multiplexer_mock.hpp"
#include "../../memory_resource_mock.hpp"
#include "../../tracking_memory_resource.hpp"

#include <limits>
#include <gmock/gmock.h>

namespace
{
using namespace libcyphal;
using namespace libcyphal::transport;
using namespace libcyphal::transport::can;

using testing::_;
using testing::Eq;
using testing::Return;
using testing::IsNull;
using testing::NotNull;
using testing::Optional;
using testing::StrictMock;
using testing::VariantWith;

class TestCanTransport : public testing::Test
{
protected:
    void SetUp() override
    {
        EXPECT_CALL(media_mock_, getMtu()).WillRepeatedly(Return(CANARD_MTU_MAX));
    }

    void TearDown() override
    {
        // TODO: Uncomment this when PMR deleter is fixed.
        // EXPECT_EQ(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    // MARK: Data members:

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

    auto maybe_transport = makeTransport(mr_mock, mux_mock_, {&media_mock_}, 0, {});
    EXPECT_THAT(maybe_transport, VariantWith<FactoryError>(VariantWith<MemoryError>(_)));
}

TEST_F(TestCanTransport, makeTransport_getLocalNodeId)
{
    // Anonymous node
    {
        auto maybe_transport = makeTransport(mr_, mux_mock_, {&media_mock_}, 0, {});
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));

        auto transport = cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
        EXPECT_THAT(transport->getLocalNodeId(), Eq(cetl::nullopt));
    }

    // Node with ID
    {
        const auto node_id = cetl::make_optional(static_cast<NodeId>(42));

        auto maybe_transport = makeTransport(mr_, mux_mock_, {&media_mock_}, 0, node_id);
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));

        auto transport = cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
        EXPECT_THAT(transport->getLocalNodeId(), Optional(42));
    }

    // Two media interfaces
    {
        StrictMock<MediaMock> media_mock2;
        EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_MAX));

        auto maybe_transport = makeTransport(mr_, mux_mock_, {&media_mock_, nullptr, &media_mock2}, 0, {});
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));
    }

    // All 3 maximum number of media interfaces
    {
        StrictMock<MediaMock> media_mock2{}, media_mock3{};
        EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_MAX));
        EXPECT_CALL(media_mock3, getMtu()).WillRepeatedly(Return(CANARD_MTU_MAX));

        auto maybe_transport = makeTransport(mr_, mux_mock_, {&media_mock_, &media_mock2, &media_mock3}, 0, {});
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<ICanTransport>>(NotNull()));
    }
}

TEST_F(TestCanTransport, makeTransport_with_invalid_arguments)
{
    // No media
    {
        const auto node_id = cetl::make_optional(static_cast<NodeId>(CANARD_NODE_ID_MAX));

        const auto maybe_transport = makeTransport(mr_, mux_mock_, {}, 0, node_id);
        EXPECT_THAT(maybe_transport, VariantWith<FactoryError>(VariantWith<ArgumentError>(_)));
    }

    // try just a bit bigger than max canard id (aka 128)
    {
        const auto node_id = cetl::make_optional(static_cast<NodeId>(CANARD_NODE_ID_MAX + 1));

        const auto maybe_transport = makeTransport(mr_, mux_mock_, {&media_mock_}, 0, node_id);
        EXPECT_THAT(maybe_transport, VariantWith<FactoryError>(VariantWith<ArgumentError>(_)));
    }

    // magic 255 number (aka CANARD_NODE_ID_UNSET) can't be used as well
    {
        const auto node_id = cetl::make_optional(static_cast<NodeId>(CANARD_NODE_ID_UNSET));

        const auto maybe_transport = makeTransport(mr_, mux_mock_, {&media_mock_}, 0, node_id);
        EXPECT_THAT(maybe_transport, VariantWith<FactoryError>(VariantWith<ArgumentError>(_)));
    }

    // just in case try 0x100 (aka overflow)
    {
        const NodeId too_big = static_cast<NodeId>(std::numeric_limits<CanardNodeID>::max()) + 1;
        const auto   node_id = cetl::make_optional(too_big);

        const auto maybe_transport = makeTransport(mr_, mux_mock_, {&media_mock_}, 0, node_id);
        EXPECT_THAT(maybe_transport, VariantWith<FactoryError>(VariantWith<ArgumentError>(_)));
    }
}

TEST_F(TestCanTransport, getProtocolParams)
{
    StrictMock<MediaMock> media_mock2{};
    EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(Return(CANARD_MTU_MAX));

    auto transport =
        cetl::get<UniquePtr<ICanTransport>>(makeTransport(mr_, mux_mock_, {&media_mock_, &media_mock2}, 0, {}));

    EXPECT_CALL(media_mock_, getMtu()).WillRepeatedly(testing::Return(CANARD_MTU_CAN_FD));
    EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(testing::Return(CANARD_MTU_CAN_CLASSIC));

    auto params = transport->getProtocolParams();
    EXPECT_THAT(params.transfer_id_modulo, Eq(1 << CANARD_TRANSFER_ID_BIT_LENGTH));
    EXPECT_THAT(params.max_nodes, Eq(CANARD_NODE_ID_MAX + 1));
    EXPECT_THAT(params.mtu_bytes, Eq(CANARD_MTU_CAN_CLASSIC));

    // Manipulate MTU values on fly
    {
        EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(testing::Return(CANARD_MTU_CAN_FD));
        EXPECT_THAT(transport->getProtocolParams().mtu_bytes, Eq(CANARD_MTU_CAN_FD));

        EXPECT_CALL(media_mock_, getMtu()).WillRepeatedly(testing::Return(CANARD_MTU_CAN_CLASSIC));
        EXPECT_THAT(transport->getProtocolParams().mtu_bytes, Eq(CANARD_MTU_CAN_CLASSIC));

        EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(testing::Return(CANARD_MTU_CAN_CLASSIC));
        EXPECT_THAT(transport->getProtocolParams().mtu_bytes, Eq(CANARD_MTU_CAN_CLASSIC));
    }
}

TEST_F(TestCanTransport, makeMessageRxSession)
{
    auto transport = cetl::get<UniquePtr<ICanTransport>>(makeTransport(mr_, mux_mock_, {&media_mock_}, 0, {}));

    auto maybe_rx_session = transport->makeMessageRxSession({42, 123});
    EXPECT_THAT(maybe_rx_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));

    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_rx_session));
    EXPECT_THAT(session->getParams().extent_bytes, Eq(42));
    EXPECT_THAT(session->getParams().subject_id, Eq(123));
}

TEST_F(TestCanTransport, makeMessageRxSession_invalid_subject_id)
{
    auto transport = cetl::get<UniquePtr<ICanTransport>>(makeTransport(mr_, mux_mock_, {&media_mock_}, 0, {}));

    auto maybe_rx_session = transport->makeMessageRxSession({0, CANARD_SUBJECT_ID_MAX + 1});
    EXPECT_THAT(maybe_rx_session, VariantWith<AnyError>(VariantWith<ArgumentError>(_)));
}

}  // namespace
