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

TEST(test_can_transport, makeTransport_getLocalNodeId)
{
    auto mr = cetl::pmr::new_delete_resource();

    StrictMock<MediaMock>       media_mock{};
    StrictMock<MultiplexerMock> multiplex_mock{};

    {
        auto maybe_transport = makeTransport(*mr, multiplex_mock, {&media_mock}, {});
        EXPECT_FALSE(cetl::get_if<FactoryError>(&maybe_transport));

        auto transport = cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
        EXPECT_TRUE(transport);
        EXPECT_EQ(cetl::nullopt, transport->getLocalNodeId());
    }

    {
        const auto node_id = cetl::make_optional(static_cast<NodeId>(42));

        auto maybe_transport = makeTransport(*mr, multiplex_mock, {}, node_id);
        EXPECT_FALSE(cetl::get_if<FactoryError>(&maybe_transport));

        auto transport = cetl::get<UniquePtr<ICanTransport>>(std::move(maybe_transport));
        EXPECT_TRUE(transport);
        EXPECT_EQ(42, transport->getLocalNodeId().value());
    }
}

TEST(test_can_transport, makeTransport_with_invalid_node_id)
{
    auto mr = cetl::pmr::new_delete_resource();

    StrictMock<MultiplexerMock> multiplex_mock{};

    // try just a bit bigger than max canard id (aka 128)
    {
        const auto node_id = cetl::make_optional(static_cast<NodeId>(CANARD_NODE_ID_MAX + 1));

        const auto maybe_transport = makeTransport(*mr, multiplex_mock, {}, node_id);
        EXPECT_TRUE(cetl::get_if<ArgumentError>(cetl::get_if<FactoryError>(&maybe_transport)));
    }

    // magic 255 number (aka CANARD_NODE_ID_UNSET) can't be used as well
    {
        const auto node_id = cetl::make_optional(static_cast<NodeId>(CANARD_NODE_ID_UNSET));

        const auto maybe_transport = makeTransport(*mr, multiplex_mock, {}, node_id);
        EXPECT_TRUE(cetl::get_if<ArgumentError>(cetl::get_if<FactoryError>(&maybe_transport)));
    }

    // just in case try 0x100 (aka overflow)
    {
        const NodeId too_big = static_cast<NodeId>(std::numeric_limits<CanardNodeID>::max()) + 1;
        const auto   node_id = cetl::make_optional(too_big);

        const auto maybe_transport = makeTransport(*mr, multiplex_mock, {}, node_id);
        EXPECT_TRUE(cetl::get_if<ArgumentError>(cetl::get_if<FactoryError>(&maybe_transport)));
    }
}

}  // namespace
