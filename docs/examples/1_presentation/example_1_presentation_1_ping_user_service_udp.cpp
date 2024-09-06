/// @file
/// Example of creating a libcyphal node in your project using posix UDP sockets and presentation layer.
/// This example demonstrates how to make client and server of a custom "Ping" user service using presentation layer
/// `Client` and `Server` classes. By custom, it means that no Nunavut generated code is used, but user's defined
/// serialization/deserialization is in use instead.
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
#include <libcyphal/presentation/client.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/presentation/response_promise.hpp>
#include <libcyphal/presentation/server.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/udp/udp_transport.hpp>
#include <libcyphal/transport/udp/udp_transport_impl.hpp>
#include <libcyphal/types.hpp>
#include <nunavut/support/serialization.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
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

/// Namespace for user's services.
///
namespace UserService
{

/// Custom "Ping" user service.
///
/// These 2 `_traits_::ExtentBytes` & `_traits_::SerializationBufferSizeBytes` constants below,
/// as well as 2 freestanding `serialize` and `deserialize` functions under this namespace - are the only requirements
/// for a user service request/response types to be automatically marshalled by libcyphal presentation layer.
///
/// @tparam IsRequest True if this is a request, false if this is a response.
///                   It's made as template just to save space (reuse the same definition for both cases).
///                   Normally you would have separate request and response types, with their own
///                   traits and serialization/deserialization functions.
///
template <bool IsRequest>
struct Ping final
{
    static constexpr PortId ServiceId = 147;

    struct _traits_
    {
        static constexpr std::size_t ExtentBytes                  = sizeof(std::uint64_t);
        static constexpr std::size_t SerializationBufferSizeBytes = sizeof(std::uint64_t);
    };

    std::uint64_t id{};  // NOLINT

    nunavut::support::SerializeResult serialize(nunavut::support::bitspan out_buffer) const
    {
        const auto result = out_buffer.setUxx(id, 64U);
        if (not result)
        {
            return -result.error();
        }
        out_buffer.add_offset(64U);
        return out_buffer.offset_bytes_ceil();
    }

    nunavut::support::SerializeResult deserialize(nunavut::support::const_bitspan in_buffer)
    {
        id = in_buffer.getU64(64U);
        in_buffer.add_offset(64U);
        return {std::min<std::size_t>(64U, in_buffer.size()) / 8U};
    }
};
using PingRequest  = Ping<true>;
using PongResponse = Ping<false>;

template <bool IsRequest>
nunavut::support::SerializeResult serialize(const Ping<IsRequest>& ping, nunavut::support::bitspan out_buffer)
{
    return ping.serialize(out_buffer);
}
template <bool IsRequest>
nunavut::support::SerializeResult deserialize(Ping<IsRequest>& ping, nunavut::support::const_bitspan in_buffer)
{
    return ping.deserialize(in_buffer);
}

}  // namespace UserService

class Example_1_Presentation_1_PingUserService_Udp : public testing::Test
{
protected:
    using Callback             = libcyphal::IExecutor::Callback;
    using Duration             = libcyphal::Duration;
    using TimePoint            = libcyphal::TimePoint;
    using UdpTransportPtr      = libcyphal::UniquePtr<IUdpTransport>;

    void SetUp() override
    {
        // Duration in seconds for which the test will run. Default is 10 seconds.
        if (const auto* const run_duration_str = std::getenv("CYPHAL__RUN"))
        {
            run_duration_ = std::chrono::duration<std::int64_t>{std::strtoll(run_duration_str, nullptr, 10)};
        }
        // Duration in seconds for which the test will run. Default is 10 seconds.
        if (const auto* const print_str = std::getenv("CYPHAL__PRINT"))
        {
            print_activities_ = 0 != std::strtoll(print_str, nullptr, 10);
        }
        // Local node ID. Default is 42.
        if (const auto* const node_id_str = std::getenv("CYPHAL__NODE__ID"))
        {
            local_node_id_  = static_cast<NodeId>(std::stoul(node_id_str));
            remote_node_id_ = local_node_id_;
        }
        // Remote node ID. Default is the same as `local_node_id_`.
        if (const auto* const node_id_str = std::getenv("CYPHAL__REMOTE__NODE__ID"))
        {
            remote_node_id_ = static_cast<NodeId>(std::stoul(node_id_str));
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

    TimePoint now() const
    {
        return executor_.now();
    }

    Duration uptime() const
    {
        return executor_.now() - startup_time_;
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
    NodeId                            remote_node_id_{42};
    Duration                          run_duration_{10s};
    bool                              print_activities_{true};
    std::vector<std::string>          iface_addresses_{"127.0.0.1"};
    // NOLINTEND

};  // Example_1_Presentation_1_PingUserService_Udp

TEST_F(Example_1_Presentation_1_PingUserService_Udp, main)
{
    using PongPromise = ResponsePromise<UserService::PongResponse>;
    using PingClient  = Client<UserService::PingRequest, UserService::PongResponse>;
    using PingServer  = Server<UserService::PingRequest, UserService::PongResponse>;

    State state;

    std::cout << "Local  node ID: " << local_node_id_ << "\n";
    std::cout << "Remote node ID: " << remote_node_id_ << "\n";

    // 1. Make UDP transport with collection of media.
    //
    constexpr std::size_t tx_capacity = 16;
    state.media_collection_.make(mr_, executor_, iface_addresses_);
    auto maybe_transport = makeTransport({mr_}, executor_, state.media_collection_.span(), tx_capacity);
    ASSERT_THAT(maybe_transport, testing::VariantWith<UdpTransportPtr>(testing::NotNull()))
        << "Can't create transport.";
    state.transport_ = cetl::get<UdpTransportPtr>(std::move(maybe_transport));
    state.transport_->setLocalNodeId(local_node_id_);

    // 2. Create presentation layer object.
    //
    Presentation presentation{mr_, executor_, *state.transport_};

    // 3. Bring up "Ping" server.
    //
    auto maybe_ping_server = presentation.makeServer<UserService::PingRequest, UserService::PongResponse>(  //
        UserService::PingRequest::ServiceId,
        [this](const auto& arg, auto continuation) {
            //
            if (print_activities_)
            {
                std::cout << "🔴  Server received 'Ping' req (ping_id=" << arg.request.id
                          << ", from_node_id=" << arg.metadata.remote_node_id << ")."
                          << CommonHelpers::Printers::describeDurationInMs(arg.approx_now - startup_time_) << std::endl;
            }
            continuation(arg.approx_now + 1s, UserService::PongResponse{arg.request.id});
        });
    ASSERT_THAT(maybe_ping_server, testing::VariantWith<PingServer>(testing::_)) << "Failed to create 'Ping' server.";

    // 4. Make "Ping" client, send periodic requests, and print replies.
    //
    auto maybe_ping_client = presentation.makeClient<UserService::PingRequest, UserService::PongResponse>(  //
        remote_node_id_,
        UserService::PingRequest::ServiceId);
    ASSERT_THAT(maybe_ping_client, testing::VariantWith<PingClient>(testing::_)) << "Failed to create 'Ping' client.";
    auto ping_client = cetl::get<PingClient>(std::move(maybe_ping_client));

    // 5. Send periodic "Ping" requests, and print "Pong" replies.
    //
    struct PingPongState
    {
        TimePoint                   start;
        UserService::PingRequest    request{1000};
        cetl::optional<PongPromise> promise;
        CommonHelpers::RunningStats stats;

    } ping_pong_state;
    //
    auto request_every_1s_cb_ = executor_.registerCallback([&](const auto&) {
        //
        ++ping_pong_state.request.id;
        if (print_activities_)
        {
            std::cout << "➡️  Client sending  'Ping' req (ping_id=" << ping_pong_state.request.id
                      << ",   to_node_id=" << remote_node_id_ << ")."
                      << CommonHelpers::Printers::describeDurationInMs(uptime()) << std::endl;  // NOLINT
        }
        ping_pong_state.start = now();
        auto maybe_promise    = ping_client.request(ping_pong_state.start + 300ms, ping_pong_state.request);
        ASSERT_THAT(maybe_promise, testing::VariantWith<PongPromise>(testing::_)) << "Failed to make 'Ping' request.";
        ping_pong_state.promise.emplace(cetl::get<PongPromise>(std::move(maybe_promise)));

        ping_pong_state.promise->setCallback([&](const auto& arg) {
            //
            const auto request_duration = arg.approx_now - ping_pong_state.start;
            ping_pong_state.stats.append(static_cast<double>(  //
                std::chrono::duration_cast<std::chrono::microseconds>(request_duration).count()));

            if (const auto* const reply_ptr = cetl::get_if<PongPromise::Success>(&arg.result))
            {
                if (print_activities_)
                {
                    const auto& reply = *reply_ptr;
                    std::cout << " ⬅️ Client received 'ponG' res (pong_id=" << reply.response.id
                              << ", from_node_id=" << reply.metadata.remote_node_id << ")."
                              << CommonHelpers::Printers::describeDurationInMs(uptime()) << ", Δ "
                              << CommonHelpers::Printers::describeDurationInUs(request_duration)
                              << ", tf_id=" << reply.metadata.rx_meta.base.transfer_id << std::endl;
                }
                return;
            }
            if (print_activities_)
            {
                std::cout << " 🚫 Client 'ping' req timeout  (ping_id=" << ping_pong_state.request.id
                          << ",   to_node_id=" << remote_node_id_ << ")."
                          << CommonHelpers::Printers::describeDurationInMs(uptime()) << ", Δ "
                          << CommonHelpers::Printers::describeDurationInUs(request_duration) << std::endl;
            }
        });
    });
    request_every_1s_cb_.schedule(Callback::Schedule::Repeat{startup_time_ + 1s, 1s});

    // 6. Main loop.
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
    std::cout << "worst_callback_lateness  = " << worst_lateness.count() << " us\n";
    std::cout << "call_duration_stats_mean = " << ping_pong_state.stats.mean() << " us\n";
    std::cout << "call_duration_stats_std  ± " << ping_pong_state.stats.standardDeviation() << " us (±"
              << std::setprecision(3)
              << 100.0 * ping_pong_state.stats.standardDeviation() / ping_pong_state.stats.mean() << "%)\n";
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
