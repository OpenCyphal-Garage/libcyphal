/// @file
/// Example of creating a libcyphal node in your project using posix UDP sockets and presentation layer.
/// This example demonstrates how to publish and subscribe to raw messages using presentation layer
/// `Publisher` and `Subscriber` classes.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "../platform/common_helpers.hpp"
#include "../platform/posix/posix_single_threaded_executor.hpp"
#include "../platform/posix/udp/udp_media.hpp"
#include "../platform/tracking_memory_resource.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/presentation/publisher.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/udp/udp_transport.hpp>
#include <libcyphal/transport/udp/udp_transport_impl.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
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

class Example_10_PosixUdpPresentation : public testing::Test
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
    Duration                          run_duration_{10s};
    std::vector<std::string>          iface_addresses_{"127.0.0.1"};
    // NOLINTEND

};  // Example_10_PosixUdpPresentation

TEST_F(Example_10_PosixUdpPresentation, raw_messages)
{
    State state;

    // 1. Make UDP transport with collection of media.
    //
    constexpr std::size_t tx_capacity = 16;
    state.media_collection_.make(mr_, executor_, iface_addresses_);
    auto maybe_transport = libcyphal::transport::udp::makeTransport(  //
        {mr_},
        executor_,
        state.media_collection_.span(),
        tx_capacity);
    ASSERT_THAT(maybe_transport, testing::VariantWith<UdpTransportPtr>(testing::_)) << "Can't create transport.";
    state.transport_ = cetl::get<UdpTransportPtr>(std::move(maybe_transport));

    // 2. Create presentation layer object.
    //
    Presentation presentation{mr_, executor_, *state.transport_};

    // 3. Publish a raw message via publisher; every 2s.
    //
    const PortId subject_id      = 123;
    auto         maybe_publisher = presentation.makePublisher<void>(subject_id);
    ASSERT_THAT(maybe_publisher, testing::VariantWith<Publisher<void>>(testing::_))
        << "Can't create publisher (subject_id=" << subject_id << ").";
    auto publisher = cetl::get<Publisher<void>>(std::move(maybe_publisher));
    //
    auto publish_every_1s_cb = executor_.registerCallback([&](const auto& arg) {
        //
        std::cout << "Publishing Hello message" << std::endl;  // NOLINT
        const TimePoint              msg_deadline = arg.approx_now + 1s;
        const cetl::span<const char> message{"Hello, World!"};
        EXPECT_THAT(publisher.publish(msg_deadline,
                                      {reinterpret_cast<const cetl::byte*>(message.data()), message.size()}),
                    testing::Eq(cetl::nullopt));
    });
    publish_every_1s_cb.schedule(Callback::Schedule::Repeat{executor_.now(), 1s});

    // Main loop.
    //
    Duration        worst_lateness{0};
    const TimePoint deadline = startup_time_ + run_duration_ + 500ms;
    //
    while (executor_.now() < deadline)
    {
        const auto spin_result = executor_.spinOnce();
        worst_lateness         = std::max(worst_lateness, spin_result.worst_lateness);

        cetl::optional<libcyphal::Duration> opt_timeout;
        if (spin_result.next_exec_time.has_value())
        {
            opt_timeout = spin_result.next_exec_time.value() - executor_.now();
        }
        EXPECT_THAT(executor_.pollAwaitableResourcesFor(opt_timeout), testing::Eq(cetl::nullopt));
    }
    std::cout << "worst_callback_lateness=" << worst_lateness.count() << "us\n";
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
