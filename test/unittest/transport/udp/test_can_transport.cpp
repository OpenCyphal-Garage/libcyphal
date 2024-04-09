/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "libcyphal/transport/can/can_transport.hpp"

#include <cetl/pf17/variant.hpp>

#include <gmock/gmock.h>

namespace
{
using namespace libcyphal::transport;
using namespace libcyphal::transport::can;

TEST(test_can_transport, makeCanTransport)
{
    auto mr = cetl::pmr::null_memory_resource();
    auto maybe_transport = CanTransport::make(*mr, static_cast<NodeId>(0));

    EXPECT_EQ(nullptr, cetl::get_if<CanTransport>(&maybe_transport));
    EXPECT_NE(nullptr, cetl::get_if<FactoryError>(&maybe_transport));
}

}  // namespace
