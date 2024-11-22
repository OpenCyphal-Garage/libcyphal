/// @file
/// Example of creating a libcyphal node in your project using posix SOCKETCAN media and CAN transport.
/// This example demonstrates how to send and receive Heartbeat messages using transport layer
/// RX/TX message session classes. It also demonstrates how to bring up a "GetInfo" server by using
/// RX/TX service request/response session classes.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "platform/common_helpers.hpp"
#include "platform/linux/can/can_media.hpp"
#include "platform/linux/epoll_single_threaded_executor.hpp"
#include "platform/node_helpers.hpp"
#include "platform/tracking_memory_resource.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/can/can_transport.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{

using namespace example::platform;          // NOLINT This our main concern here in this test.
using namespace libcyphal::transport;       // NOLINT This our main concern here in this test.
using namespace libcyphal::transport::can;  // NOLINT This our main concern here in this test.

using Duration        = libcyphal::Duration;
using TimePoint       = libcyphal::TimePoint;
using CanTransportPtr = libcyphal::UniquePtr<ICanTransport>;

// https://github.com/llvm/llvm-project/issues/53444
// NOLINTBEGIN(misc-unused-using-decls, misc-include-cleaner)
using std::literals::chrono_literals::operator""s;
using std::literals::chrono_literals::operator""ms;
// NOLINTEND(misc-unused-using-decls, misc-include-cleaner)

using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::VariantWith;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class Example_0_Transport_2_Heartbeat_GetInfo_Can : public testing::Test
{
protected:
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
        // Space separated list of interface addresses, like "slcan0 slcan1". Default is "vcan0".
        if (const auto* const iface_addresses_str = std::getenv("CYPHAL__CAN__IFACE"))
        {
            iface_addresses_ = CommonHelpers::splitInterfaceAddresses(iface_addresses_str);
        }

        startup_time_ = executor_.now();
    }

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocated_bytes, 0);
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    Duration uptime() const
    {
        return executor_.now() - startup_time_;
    }

    // MARK: Data members:
    // NOLINTBEGIN

    struct State
    {
        cetl::pmr::memory_resource& mr_;
        Linux::CanMedia::Collection media_collection_{};
        CanTransportPtr             transport_{nullptr};
        NodeHelpers::Heartbeat      heartbeat_{mr_};
        NodeHelpers::GetInfo        get_info_{mr_};

    };  // State

    example::platform::TrackingMemoryResource             mr_;
    example::platform::Linux::EpollSingleThreadedExecutor executor_;
    TimePoint                                             startup_time_{};
    NodeId                                                local_node_id_{42};
    Duration                                              run_duration_{10s};
    std::vector<std::string>                              iface_addresses_{"vcan0"};
    // NOLINTEND

};  // Example_0_Transport_2_Heartbeat_GetInfo_Can

// MARK: - Tests:

TEST_F(Example_0_Transport_2_Heartbeat_GetInfo_Can, main)
{
    State state{mr_};

    // Make CAN transport with collection of media.
    //
    if (!state.media_collection_.make(mr_, executor_, iface_addresses_))
    {
        GTEST_SKIP();
    }
    CommonHelpers::Can::makeTransport(state, mr_, executor_, local_node_id_);

    // Publish/Subscribe heartbeats.
    state.heartbeat_.makeTxSession(*state.transport_, executor_, startup_time_);
    state.heartbeat_.makeRxSession(*state.transport_, [&](const auto& arg) {
        //
        state.heartbeat_.tryDeserializeAndPrint(uptime(), arg.transfer);
    });

    // Bring up 'GetInfo' server.
    state.get_info_.setName("org.opencyphal.Ex_0_Tran_2_HB_GetInfo_CAN");
    state.get_info_.makeRxSession(*state.transport_);
    state.get_info_.makeTxSession(*state.transport_);

    // Main loop.
    //
    CommonHelpers::runMainLoop(executor_, startup_time_ + run_duration_ + 500ms, [&](const auto now) {
        //
        state.get_info_.receive(now);
    });
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
