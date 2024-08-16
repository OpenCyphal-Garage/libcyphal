/// @file
/// Example of creating a libcyphal node in your project using posix UDP sockets and transport.
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
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/udp/udp_transport.hpp>
#include <libcyphal/types.hpp>

#include <uavcan/node/Health_1_0.hpp>
#include <uavcan/node/Mode_1_0.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
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

class Example_12_PosixUdpPresentation : public testing::Test
{
protected:
    using Callback        = libcyphal::IExecutor::Callback;
    using Duration        = libcyphal::Duration;
    using TimePoint       = libcyphal::TimePoint;
    using UdpTransportPtr = libcyphal::UniquePtr<IUdpTransport>;

    void SetUp() override
    {
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

    NodeHelpers::Heartbeat::Message makeHeartbeatMsg(const libcyphal::TimePoint now, const bool is_warn = false) const
    {
        const auto uptime_in_secs = std::chrono::duration_cast<std::chrono::seconds>(now - startup_time_);
        return {static_cast<std::uint32_t>(uptime_in_secs.count()),
                {is_warn ? uavcan::node::Health_1_0::WARNING : uavcan::node::Health_1_0::NOMINAL},
                {is_warn ? uavcan::node::Mode_1_0::MAINTENANCE : uavcan::node::Mode_1_0::OPERATIONAL}};
    }

    // MARK: Data members:
    // NOLINTBEGIN

    struct State
    {
        posix::UdpMedia::Collection media_collection_;
        UdpTransportPtr             transport_;
        NodeHelpers::Heartbeat      heartbeat_;
        NodeHelpers::GetInfo        get_info_;

    };  // State

    TrackingMemoryResource            mr_;
    posix::PollSingleThreadedExecutor executor_{mr_};
    TimePoint                         startup_time_{};
    NodeId                            local_node_id_{42};
    Duration                          run_duration_{10s};
    std::vector<std::string>          iface_addresses_{"127.0.0.1"};
    // NOLINTEND

};  // Example_12_PosixUdpPresentation

// MARK: - Tests:

TEST_F(Example_12_PosixUdpPresentation, heartbeat_and_getInfo)
{
    State state;

    // Make UDP transport with collection of media.
    //
    state.media_collection_.make(mr_, executor_, iface_addresses_);
    CommonHelpers::Udp::makeTransport(state, mr_, executor_, local_node_id_);

    Presentation presentation{mr_, *state.transport_};

    // Publish heartbeats.
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
    //
    // Print also received heartbeats.
    // TODO: Replace with message subscriber when it will be available.
    state.heartbeat_.makeRxSession(*state.transport_, startup_time_);

    // Bring up 'GetInfo' server.
    // TODO: Replace with service server when it will be available.
    state.get_info_.setName("org.opencyphal.example_12_posix_udp_presentation");
    state.get_info_.makeRxSession(*state.transport_);
    state.get_info_.makeTxSession(*state.transport_);

    // Main loop.
    //
    CommonHelpers::runMainLoop(executor_, startup_time_ + run_duration_ + 500ms, [&](const auto now) {
        //
        state.get_info_.receive(now);
        state.heartbeat_.receive(now);
    });
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace