/// @file
/// Example of creating a libcyphal node in your project.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "platform/posix/udp_media.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/platform/single_threaded_executor.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/svc_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/udp/media.hpp>
#include <libcyphal/transport/udp/udp_transport.hpp>
#include <libcyphal/transport/udp/udp_transport_impl.hpp>
#include <libcyphal/types.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <thread>
#include <utility>

namespace
{

using namespace libcyphal::transport;       // NOLINT This our main concern here in this test.
using namespace libcyphal::transport::udp;  // NOLINT This our main concern here in this test.

using UdpTransportPtr     = libcyphal::UniquePtr<libcyphal::transport::udp::IUdpTransport>;
using RequestTxSessionPtr = libcyphal::UniquePtr<libcyphal::transport::IRequestTxSession>;

using std::literals::chrono_literals::operator""s;
using std::literals::chrono_literals::operator""ms;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

TEST(example_02_transport_svc, posix_udp)
{
    auto& general_mr = *cetl::pmr::new_delete_resource();

    example::platform::posix::UdpMedia udp_media{general_mr};

    libcyphal::platform::SingleThreadedExecutor executor{general_mr};

    const std::size_t           tx_capacity = 16;
    std::array<udp::IMedia*, 1> media_array{&udp_media};

    auto maybe_transport = udp::makeTransport({general_mr}, executor, media_array, tx_capacity);
    if (nullptr != cetl::get_if<FactoryFailure>(&maybe_transport))
    {
        FAIL() << "Failed to create transport.";
    }
    auto udp_transport = cetl::get<UdpTransportPtr>(std::move(maybe_transport));
    udp_transport->setLocalNodeId(0x13);

    auto maybe_req_tx_session = udp_transport->makeRequestTxSession({42, 0x31});
    if (nullptr != cetl::get_if<AnyFailure>(&maybe_req_tx_session))
    {
        FAIL() << "Failed to create request tx session.";
    }
    auto req_tx_session = cetl::get<RequestTxSessionPtr>(std::move(maybe_req_tx_session));
    req_tx_session->setSendTimeout(1000s);

    TransferId                             transfer_id{0};
    libcyphal::IExecutor::Callback::Handle periodic;
    periodic = executor.registerCallback([&](const auto now) {  //
        //
        ++transfer_id;
        std::cout << "Sending transfer " << transfer_id << std::endl;  // NOLINT
        const PayloadFragments empty_payload{};
        req_tx_session->send({transfer_id, now, Priority::Nominal}, empty_payload);
        periodic.scheduleAt(now + 500ms);
    });
    periodic.scheduleAt({});

    const auto deadline = executor.now() + 5s;
    while (executor.now() < deadline)
    {
        executor.spinOnce();

        std::this_thread::sleep_for(10ms);
    }

    periodic.reset();
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
