/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "media_mock.hpp"
#include "../multiplexer_mock.hpp"
#include <libcyphal/transport/can/transport.hpp>

#include <cetl/pf17/variant.hpp>

#include <gmock/gmock.h>

namespace
{
using namespace libcyphal;
using namespace libcyphal::transport;
using namespace libcyphal::transport::can;

using testing::StrictMock;

TEST(test_can_transport, CanTransport_make)
{
    auto mr = cetl::pmr::new_delete_resource();

    StrictMock<MediaMock>       media_mock{};
    StrictMock<MultiplexerMock> multiplex_mock{};

    auto maybe_transport = Transport::make(*mr, multiplex_mock, {}, static_cast<NodeId>(0));
    {
        auto transport = cetl::get<libcyphal::UniquePtr<Transport>>(std::move(maybe_transport));
        EXPECT_EQ(nullptr, cetl::get_if<FactoryError>(&maybe_transport));

        EXPECT_NE(nullptr, transport);
        EXPECT_EQ(cetl::nullopt, transport->getLocalNodeId());
    }
}

}  // namespace
