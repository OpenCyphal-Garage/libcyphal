/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <libcyphal/transport/udp/transport.hpp>

#include "media_mock.hpp"
#include "../multiplexer_mock.hpp"

#include <gmock/gmock.h>

namespace
{
using namespace libcyphal;
using namespace libcyphal::transport;
using namespace libcyphal::transport::udp;

using testing::IsNull;
using testing::NotNull;
using testing::StrictMock;

// MARK: Tests:

TEST(test_udp_transport, makeTransport)
{
    auto mr = cetl::pmr::new_delete_resource();

    StrictMock<MediaMock>       media_mock{};
    StrictMock<MultiplexerMock> multiplex_mock{};

    {
        auto maybe_transport = makeTransport(*mr, multiplex_mock, {&media_mock}, {});

        EXPECT_THAT(cetl::get_if<UniquePtr<IUdpTransport>>(&maybe_transport), IsNull());
        EXPECT_THAT(cetl::get_if<NotImplementedError>(cetl::get_if<FactoryError>(&maybe_transport)), NotNull());
    }
}

}  // namespace
