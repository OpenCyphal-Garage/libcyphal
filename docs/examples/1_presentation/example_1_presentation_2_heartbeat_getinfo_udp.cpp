/// @file
/// Example of creating a libcyphal node in your project using posix UDP sockets and presentation layer.
/// This example demonstrates how to publish and subscribe to the Heartbeat messages using presentation layer
/// `Publisher` and `Subscriber` classes. It also demonstrates how to bring up a `GetInfo` server.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "platform/common_helpers.hpp"
#include "platform/node_helpers.hpp"
#include "platform/posix/posix_single_threaded_executor.hpp"
#include "platform/posix/udp/udp_media.hpp"
#include "platform/tracking_memory_resource.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/presentation/server.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/udp/udp_transport.hpp>
#include <libcyphal/types.hpp>

#include <uavcan/node/GetInfo_1_0.hpp>
#include <uavcan/node/Health_1_0.hpp>
#include <uavcan/node/Mode_1_0.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace
{

using namespace example::platform;          // NOLINT This our main concern here in this test.
using namespace libcyphal::presentation;    // NOLINT This our main concern here in this test.
using namespace libcyphal::transport;       // NOLINT This our main concern here in this test.
using namespace libcyphal::transport::udp;  // NOLINT This our main concern here in this test.

// https://github.com/llvm/llvm-project/issues/53444
// NOLINTBEGIN(misc-unused-using-decls, misc-include-cleaner)
using std::literals::chrono_literals::operator""s;
using std::literals::chrono_literals::operator""ms;
// NOLINTEND(misc-unused-using-decls, misc-include-cleaner)

using testing::IsEmpty;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class Example_1_Presentation_2_Heartbeat_GetInfo_Udp : public testing::Test
{
protected:
    using Callback        = libcyphal::IExecutor::Callback;
    using Duration        = libcyphal::Duration;
    using TimePoint       = libcyphal::TimePoint;
    using UdpTransportPtr = libcyphal::UniquePtr<IUdpTransport>;

    void SetUp() override
    {
        cetl::pmr::set_default_resource(&mr_);

        // Duration in seconds for which the test will run. Default is 10 seconds.
        if (const auto* const run_duration_str = std::getenv("CYPHAL__RUN"))
        {
            run_duration_ = std::chrono::duration<std::int64_t>{std::strtoll(run_duration_str, nullptr, 10)};
        }
        // Local node ID. Default is 42.
        if (const auto* const node_id_str = std::getenv("CYPHAL__NODE__ID"))
        {
            local_node_id_ = static_cast<NodeId>(std::stoul(node_id_str));
        }
        // Space separated list of interface addresses, like "127.0.0.1 192.168.1.162". Default is "127.0.0.1".
        if (const auto* const iface_addresses_str = std::getenv("CYPHAL__UDP__IFACE"))
        {
            iface_addresses_ = CommonHelpers::splitInterfaceAddresses(iface_addresses_str);
        }

        startup_time_ = executor_.now();
    }

    void TearDown() override
    {
        executor_.releaseTemporaryResources();

        EXPECT_THAT(mr_.allocated_bytes, 0);
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    NodeHelpers::Heartbeat::Message makeHeartbeatMsg(const libcyphal::TimePoint now, const bool is_warn = false)
    {
        using Message = NodeHelpers::Heartbeat::Message;

        Message message{mr_alloc_};

        const auto uptime_in_secs = std::chrono::duration_cast<std::chrono::seconds>(now - startup_time_);

        message.uptime       = static_cast<std::uint32_t>(uptime_in_secs.count());
        message.health.value = is_warn ? uavcan::node::Health_1_0::WARNING : uavcan::node::Health_1_0::NOMINAL;
        message.mode.value   = is_warn ? uavcan::node::Mode_1_0::MAINTENANCE : uavcan::node::Mode_1_0::OPERATIONAL;

        return message;
    }

    // MARK: Data members:
    // NOLINTBEGIN

    struct State
    {
        cetl::pmr::memory_resource&            mr_;
        posix::UdpMedia::Collection            media_collection_{};
        UdpTransportPtr                        transport_{nullptr};
        NodeHelpers::Heartbeat                 heartbeat_{mr_};
        cetl::pmr::polymorphic_allocator<void> mr_alloc_{&mr_};
        uavcan::node::GetInfo_1_0::Response    get_info_response{mr_alloc_};

    };  // State

    TrackingMemoryResource                 mr_;
    posix::PollSingleThreadedExecutor      executor_{mr_};
    TimePoint                              startup_time_{};
    NodeId                                 local_node_id_{42};
    Duration                               run_duration_{10s};
    std::vector<std::string>               iface_addresses_{"127.0.0.1"};
    cetl::pmr::polymorphic_allocator<void> mr_alloc_{&mr_};
    // NOLINTEND

};  // Example_1_Presentation_2_Heartbeat_GetInfo_Udp

// MARK: - Tests:

TEST_F(Example_1_Presentation_2_Heartbeat_GetInfo_Udp, main)
{
    State state{mr_};
    state.get_info_response.protocol_version.major = 1;

    // Make UDP transport with collection of media.
    //
    state.media_collection_.make(mr_, executor_, iface_addresses_);
    CommonHelpers::Udp::makeTransport(state, mr_, executor_, local_node_id_);

    Presentation presentation{mr_, executor_, *state.transport_};

    // Publish heartbeats.
    //
    auto heartbeat_publisher = NodeHelpers::Heartbeat::makePublisher(presentation);
    ASSERT_THAT(heartbeat_publisher, testing::Optional(testing::_));
    auto publish_every_1s_cb = executor_.registerCallback([&](const auto& arg) {
        //
        EXPECT_THAT(heartbeat_publisher->publish(arg.approx_now + 1s, makeHeartbeatMsg(arg.approx_now)),
                    testing::Eq(cetl::nullopt));
    });
    //
    constexpr auto period = std::chrono::seconds{NodeHelpers::Heartbeat::Message::MAX_PUBLICATION_PERIOD};
    publish_every_1s_cb.schedule(Callback::Schedule::Repeat{startup_time_ + period, period});

    // Subscribe and print received heartbeats.
    //
    auto heartbeat_subscriber = NodeHelpers::Heartbeat::makeSubscriber(presentation);
    ASSERT_THAT(heartbeat_subscriber, testing::Optional(testing::_));
    heartbeat_subscriber->setOnReceiveCallback([&](const auto& arg) {
        //
        NodeHelpers::Heartbeat::print(arg.approx_now - startup_time_, arg.message, arg.metadata);
    });

    // Bring up 'GetInfo' server.
    //
    using uavcan::node::GetInfo_1_0;
    const std::string node_name{"org.opencyphal.Ex_1_Pres_2_HB_GetInfo_UDP"};
    std::copy_n(node_name.begin(), std::min(node_name.size(), 50UL), std::back_inserter(state.get_info_response.name));
    //
    auto maybe_get_info_srv = presentation.makeServer<GetInfo_1_0>([&state](const auto& arg, auto continuation) {
        //
        std::cout << "ⓘ  Received 'GetInfo' request (from_node_id=" << arg.metadata.remote_node_id << ")."
                  << std::endl;  // NOLINT
        continuation(arg.approx_now + 1s, state.get_info_response);
    });
    ASSERT_THAT(maybe_get_info_srv, testing::VariantWith<ServiceServer<GetInfo_1_0>>(testing::_))
        << "Can't create 'GetInfo' server.";

    // Main loop.
    //
    CommonHelpers::runMainLoop(executor_, startup_time_ + run_duration_ + 500ms, [](const auto) {});
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
