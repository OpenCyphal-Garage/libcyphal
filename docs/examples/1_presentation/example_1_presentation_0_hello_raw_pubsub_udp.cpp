/// @file
/// Example of creating a libcyphal node in your project using posix UDP sockets and presentation layer.
/// This example demonstrates how to publish and subscribe to raw "Hello" messages using presentation layer
/// `Publisher` and `Subscriber` classes.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "platform/common_helpers.hpp"
#include "platform/posix/posix_single_threaded_executor.hpp"
#include "platform/posix/udp/udp_media.hpp"
#include "platform/tracking_memory_resource.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/presentation/publisher.hpp>
#include <libcyphal/presentation/subscriber.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/udp/udp_transport.hpp>
#include <libcyphal/transport/udp/udp_transport_impl.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
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

class Example_1_Presentation_0_HelloRawPubSub_Udp : public testing::Test
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

    // MARK: Data members:
    // NOLINTBEGIN

    struct State
    {
        posix::UdpMedia::Collection media_collection_;
        UdpTransportPtr             transport_;

    };  // State

    TrackingMemoryResource            mr_;
    posix::PollSingleThreadedExecutor executor_{mr_};
    TimePoint                         startup_time_{};
    NodeId                            local_node_id_{42};
    Duration                          run_duration_{10s};
    std::vector<std::string>          iface_addresses_{"127.0.0.1"};
    // NOLINTEND

};  // Example_1_Presentation_0_HelloRawPubSub_Udp

TEST_F(Example_1_Presentation_0_HelloRawPubSub_Udp, main)
{
    State state;

    // 1. Make UDP transport with collection of media.
    //
    constexpr std::size_t tx_capacity = 16;
    state.media_collection_.make(mr_, executor_, iface_addresses_);
    auto maybe_transport = makeTransport({mr_}, executor_, state.media_collection_.span(), tx_capacity);
    ASSERT_THAT(maybe_transport, testing::VariantWith<UdpTransportPtr>(testing::NotNull()))
        << "Can't create transport.";
    state.transport_ = cetl::get<UdpTransportPtr>(std::move(maybe_transport));
    state.transport_->setLocalNodeId(local_node_id_);
    state.transport_->setTransientErrorHandler(CommonHelpers::Udp::transientErrorReporter);

    // 2. Create presentation layer object.
    //
    Presentation presentation{mr_, executor_, *state.transport_};

    // 3. Publish a raw message via publisher; every 1s.
    //
    constexpr PortId test_subject_id = 147;
    auto             maybe_publisher = presentation.makePublisher<void>(test_subject_id);
    ASSERT_THAT(maybe_publisher, testing::VariantWith<Publisher<void>>(testing::_))
        << "Can't create publisher (subject_id=" << test_subject_id << ").";
    auto raw_publisher = cetl::get<Publisher<void>>(std::move(maybe_publisher));
    //
    std::size_t publish_msg_count   = 0;
    auto        publish_every_1s_cb = executor_.registerCallback([&](const auto& arg) {
        //
        ++publish_msg_count;
        std::cout << "📨 Publishing Hello message # " << publish_msg_count << std::endl;  // NOLINT
        const TimePoint                  msg_deadline = arg.approx_now + 1s;
        constexpr cetl::span<const char> message{"Hello, World!"};
        //
        const std::array<const PayloadFragment, 1> payload_fragments{
            PayloadFragment{reinterpret_cast<const cetl::byte*>(message.data()),  // NOLINT
                                         message.size()}};
        EXPECT_THAT(raw_publisher.publish(msg_deadline, payload_fragments), testing::Eq(cetl::nullopt));
    });
    publish_every_1s_cb.schedule(Callback::Schedule::Repeat{executor_.now() + 1s, 1s});

    // 4. Subscribe to raw messages via subscriber.
    //
    constexpr std::size_t extent_bytes     = 16;
    auto                  maybe_subscriber = presentation.makeSubscriber(test_subject_id, extent_bytes);
    ASSERT_THAT(maybe_subscriber, testing::VariantWith<Subscriber<void>>(testing::_))
        << "Can't create subscriber (subject_id=" << test_subject_id << ").";
    auto raw_subscriber = cetl::get<Subscriber<void>>(std::move(maybe_subscriber));
    //
    std::size_t received_msg_count = 0;
    raw_subscriber.setOnReceiveCallback([&](const auto& arg) {
        //
        ++received_msg_count;
        std::array<char, extent_bytes + 1> message{};
        const auto                         msg_size = arg.raw_message.copy(0, message.data(), extent_bytes);
        std::cout << "🔵 Received message '" << message.data() << "' (bytes=" << msg_size
                  << ", msg_cnt=" << received_msg_count << ", node=" << arg.metadata.publisher_node_id.value_or(-1)
                  << ")." << std::endl;
    });

    // 5. Main loop.
    //
    Duration        worst_lateness{0};
    const TimePoint deadline = startup_time_ + run_duration_ + 500ms;
    std::cout << "-----------\nRunning..." << std::endl;  // NOLINT
    //
    while (executor_.now() < deadline)
    {
        const auto spin_result = executor_.spinOnce();
        worst_lateness         = std::max(worst_lateness, spin_result.worst_lateness);

        cetl::optional<libcyphal::Duration> opt_timeout{1s};  // awake at least once per second
        if (spin_result.next_exec_time.has_value())
        {
            opt_timeout = std::min(*opt_timeout, spin_result.next_exec_time.value() - executor_.now());
        }
        EXPECT_THAT(executor_.pollAwaitableResourcesFor(opt_timeout), testing::Eq(cetl::nullopt));
    }

    std::cout << "Done.\n-----------\nStats:\n";
    std::cout << "worst_callback_lateness=" << worst_lateness.count() << "us\n";
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
