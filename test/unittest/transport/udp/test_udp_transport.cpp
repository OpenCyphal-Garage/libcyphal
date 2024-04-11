/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "media_mock.hpp"
#include "../multiplexer_mock.hpp"
#include <libcyphal/transport/udp/transport.hpp>

#include <cetl/pf17/variant.hpp>

#include <gmock/gmock.h>

namespace
{
using namespace libcyphal;
using namespace libcyphal::transport;
using namespace libcyphal::transport::udp;

using testing::StrictMock;

TEST(test_udp_transport, factory_make)
{
    auto mr = cetl::pmr::new_delete_resource();

    StrictMock<MediaMock>       media_mock{};
    StrictMock<MultiplexerMock> multiplex_mock{};

    auto maybe_transport = makeTransport(*mr, multiplex_mock, {&media_mock}, static_cast<NodeId>(0));

    auto transport = cetl::get_if<UniquePtr<IUdpTransport>>(&maybe_transport);
    EXPECT_EQ(nullptr, transport);
    auto error = cetl::get_if<FactoryError>(&maybe_transport);
    EXPECT_NE(nullptr, error);
    EXPECT_NE(nullptr, cetl::get_if<NotImplementedError>(error));
}

}  // namespace
