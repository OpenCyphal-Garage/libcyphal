/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "media_mock.hpp"
#include "../multiplexer_mock.hpp"
#include <libcyphal/transport/can/transport.hpp>

#include <cetl/pf17/variant.hpp>

#include <limits>
#include <gmock/gmock.h>

namespace
{
using namespace libcyphal;
using namespace libcyphal::transport;
using namespace libcyphal::transport::can;

using testing::StrictMock;

class TestCanTransport : public testing::Test
{
protected:
    StrictMock<MediaMock>       media_mock_{};
    StrictMock<MultiplexerMock> mux_mock_{};
    cetl::pmr::memory_resource& mr_{*cetl::pmr::new_delete_resource()};
};

// MARK: Tests:

TEST_F(TestCanTransport, makeTransport_getLocalNodeId)
{
    // Anonymous node
    {
        auto maybe_transport = makeTransport(mr_, mux_mock_, {&media_mock_}, {});
        EXPECT_FALSE(cetl::get_if<FactoryError>(&maybe_transport));

        auto transport = cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
        EXPECT_TRUE(transport);
        EXPECT_EQ(cetl::nullopt, transport->getLocalNodeId());
    }

    // Node with ID
    {
        const auto node_id = cetl::make_optional(static_cast<NodeId>(42));

        auto maybe_transport = makeTransport(mr_, mux_mock_, {&media_mock_}, node_id);
        EXPECT_FALSE(cetl::get_if<FactoryError>(&maybe_transport));

        auto transport = cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
        EXPECT_TRUE(transport);
        EXPECT_EQ(42, transport->getLocalNodeId().value());
    }

    // Two media interfaces
    {
        StrictMock<MediaMock> media_mock2;

        auto maybe_transport = makeTransport(mr_, mux_mock_, {&media_mock_, nullptr, &media_mock2}, {});
        EXPECT_FALSE(cetl::get_if<FactoryError>(&maybe_transport));

        auto transport = cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
        EXPECT_TRUE(transport);
    }

    // All 3 maximum number of media interfaces
    {
        StrictMock<MediaMock> media_mock2{}, media_mock3{};

        auto maybe_transport = makeTransport(mr_, mux_mock_, {&media_mock_, &media_mock2, &media_mock3}, {});
        EXPECT_FALSE(cetl::get_if<FactoryError>(&maybe_transport));

        auto transport = cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
        EXPECT_TRUE(transport);
    }
}

TEST_F(TestCanTransport, makeTransport_with_invalid_arguments)
{
    // No media
    {
        const auto node_id = cetl::make_optional(static_cast<NodeId>(CANARD_NODE_ID_MAX));

        const auto maybe_transport = makeTransport(mr_, mux_mock_, {}, node_id);
        EXPECT_TRUE(cetl::get_if<ArgumentError>(cetl::get_if<FactoryError>(&maybe_transport)));
    }

    // try just a bit bigger than max canard id (aka 128)
    {
        const auto node_id = cetl::make_optional(static_cast<NodeId>(CANARD_NODE_ID_MAX + 1));

        const auto maybe_transport = makeTransport(mr_, mux_mock_, {&media_mock_}, node_id);
        EXPECT_TRUE(cetl::get_if<ArgumentError>(cetl::get_if<FactoryError>(&maybe_transport)));
    }

    // magic 255 number (aka CANARD_NODE_ID_UNSET) can't be used as well
    {
        const auto node_id = cetl::make_optional(static_cast<NodeId>(CANARD_NODE_ID_UNSET));

        const auto maybe_transport = makeTransport(mr_, mux_mock_, {&media_mock_}, node_id);
        EXPECT_TRUE(cetl::get_if<ArgumentError>(cetl::get_if<FactoryError>(&maybe_transport)));
    }

    // just in case try 0x100 (aka overflow)
    {
        const NodeId too_big = static_cast<NodeId>(std::numeric_limits<CanardNodeID>::max()) + 1;
        const auto   node_id = cetl::make_optional(too_big);

        const auto maybe_transport = makeTransport(mr_, mux_mock_, {&media_mock_}, node_id);
        EXPECT_TRUE(cetl::get_if<ArgumentError>(cetl::get_if<FactoryError>(&maybe_transport)));
    }
}

TEST_F(TestCanTransport, getProtocolParams)
{
    StrictMock<MediaMock> media_mock2{};

    auto transport =
        cetl::get<UniquePtr<ICanTransport>>(makeTransport(mr_, mux_mock_, {&media_mock_, &media_mock2}, {}));

    EXPECT_CALL(media_mock_, getMtu()).WillRepeatedly(testing::Return(CANARD_MTU_CAN_FD));
    EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(testing::Return(CANARD_MTU_CAN_CLASSIC));

    auto params = transport->getProtocolParams();
    EXPECT_EQ(1 << CANARD_TRANSFER_ID_BIT_LENGTH, params.transfer_id_modulo);
    EXPECT_EQ(CANARD_NODE_ID_MAX + 1, params.max_nodes);
    EXPECT_EQ(CANARD_MTU_CAN_CLASSIC, params.mtu_bytes);

    // Manipulate MTU values on fly
    {
        EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(testing::Return(CANARD_MTU_CAN_FD));
        EXPECT_EQ(CANARD_MTU_CAN_FD, transport->getProtocolParams().mtu_bytes);

        EXPECT_CALL(media_mock_, getMtu()).WillRepeatedly(testing::Return(CANARD_MTU_CAN_CLASSIC));
        EXPECT_EQ(CANARD_MTU_CAN_CLASSIC, transport->getProtocolParams().mtu_bytes);

        EXPECT_CALL(media_mock2, getMtu()).WillRepeatedly(testing::Return(CANARD_MTU_CAN_CLASSIC));
        EXPECT_EQ(CANARD_MTU_CAN_CLASSIC, transport->getProtocolParams().mtu_bytes);
    }
}

TEST_F(TestCanTransport, makeMessageRxSession)
{
    auto transport = cetl::get<UniquePtr<ICanTransport>>(makeTransport(mr_, mux_mock_, {&media_mock_}, {}));

    auto maybe_rx_session = transport->makeMessageRxSession({42, 123});
    EXPECT_FALSE(cetl::get_if<AnyError>(&maybe_rx_session));

    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_rx_session));
    EXPECT_TRUE(session);
    EXPECT_EQ(42, session->getParams().extent_bytes);
    EXPECT_EQ(123, session->getParams().subject_id);
}

TEST_F(TestCanTransport, makeMessageRxSession_invalid_subject_id)
{
    auto transport = cetl::get<UniquePtr<ICanTransport>>(makeTransport(mr_, mux_mock_, {&media_mock_}, {}));

    auto maybe_rx_session = transport->makeMessageRxSession({0, CANARD_SUBJECT_ID_MAX + 1});
    EXPECT_TRUE(cetl::get_if<ArgumentError>(cetl::get_if<AnyError>(&maybe_rx_session)));
}

}  // namespace
