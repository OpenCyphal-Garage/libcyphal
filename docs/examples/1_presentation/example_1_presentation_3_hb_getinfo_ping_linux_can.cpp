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
#include "platform/linux/can/can_media.hpp"
#include "platform/linux/epoll_single_threaded_executor.hpp"
#include "platform/tracking_memory_resource.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/presentation/client.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/presentation/publisher.hpp>
#include <libcyphal/presentation/response_promise.hpp>
#include <libcyphal/presentation/server.hpp>
#include <libcyphal/transport/can/can_transport.hpp>
#include <libcyphal/transport/can/can_transport_impl.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <nunavut/support/serialization.hpp>
#include <uavcan/node/GetInfo_1_0.hpp>
#include <uavcan/node/Heartbeat_1_0.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace
{

using namespace example::platform;          // NOLINT This our main concern here in this test.
using namespace libcyphal::presentation;    // NOLINT This our main concern here in this test.
using namespace libcyphal::transport;       // NOLINT This our main concern here in this test.
using namespace libcyphal::transport::can;  // NOLINT This our main concern here in this test.

// https://github.com/llvm/llvm-project/issues/53444
// NOLINTBEGIN(misc-unused-using-decls, misc-include-cleaner)
using std::literals::chrono_literals::operator""s;
using std::literals::chrono_literals::operator""ms;
using std::literals::chrono_literals::operator""us;
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
    using allocator_type = cetl::pmr::polymorphic_allocator<void>;

    static constexpr PortId ServiceId = 147;

    struct _traits_
    {
        static constexpr std::size_t ExtentBytes                  = sizeof(std::uint64_t);
        static constexpr std::size_t SerializationBufferSizeBytes = sizeof(std::uint64_t);
    };

    Ping() = default;

    explicit Ping(const std::uint64_t _id)
        : id{_id}
    {
    }

    // Allocator constructor
    explicit Ping(const allocator_type& allocator)
    {
        (void) allocator;
    }

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

class Example_1_Presentation_3_HB_GetInfo_Ping_Can : public testing::Test
{
protected:
    using Callback        = libcyphal::IExecutor::Callback;
    using Duration        = libcyphal::Duration;
    using TimePoint       = libcyphal::TimePoint;
    using CanTransportPtr = libcyphal::UniquePtr<ICanTransport>;
    using PongPromise     = ResponsePromise<UserService::PongResponse>;

    void SetUp() override
    {
        cetl::pmr::set_default_resource(&mr_);

        // Duration in seconds for which the test will run. Default is 10 seconds.
        if (const auto* const run_duration_str = std::getenv("CYPHAL__RUN"))
        {
            run_duration_ = std::chrono::duration<std::int64_t>{std::strtoll(run_duration_str, nullptr, 10)};
        }
        // Boolean (0 is `false`, otherwise `true`) which turns on/off printing of intermediate activities.
        // Default is `true`.
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

    uavcan::node::Heartbeat_1_0 makeHeartbeatMsg(const libcyphal::TimePoint now)
    {
        using Message = uavcan::node::Heartbeat_1_0;

        Message message{{&mr_}};

        const auto uptime_in_secs = std::chrono::duration_cast<std::chrono::seconds>(now - startup_time_);
        message.uptime            = static_cast<std::uint32_t>(uptime_in_secs.count());

        return message;
    }

    TimePoint now() const
    {
        return executor_.now();
    }

    Duration uptime() const
    {
        return executor_.now() - startup_time_;
    }

    struct PingPongState
    {
        cetl::pmr::memory_resource&  mr;
        const std::string            name;
        CommonHelpers::RunningStats& stats;
        UserService::PingRequest     request{UserService::PingRequest::allocator_type{&mr}};
        Priority                     priority;
        TimePoint                    req_start;
        cetl::optional<PongPromise>  promise;
    };
    void processPingPongResult(PingPongState& state, const PongPromise::Callback::Arg& arg)
    {
        const auto request_duration = arg.approx_now - state.req_start;

        if (const auto* const reply_ptr = cetl::get_if<PongPromise::Success>(&arg.result))
        {
            state.stats.append(static_cast<double>(  //
                std::chrono::duration_cast<std::chrono::microseconds>(request_duration).count()));

            if (print_activities_)
            {
                const auto& reply = *reply_ptr;
                std::cout << " ⬅️ Client '" << state.name << "' received 'ponG' res (pong_id=" << reply.response.id
                          << ", from_node_id=" << reply.metadata.remote_node_id << ")."
                          << CommonHelpers::Printers::describeDurationInMs(uptime()) << ", Δ "
                          << CommonHelpers::Printers::describeDurationInUs(request_duration)
                          << ", tf_id=" << reply.metadata.rx_meta.base.transfer_id << std::endl;  // NOLINT
            }
            return;
        }
        if (print_activities_)
        {
            std::cout << " 🔴 Client '" << state.name << "' timeout  'ping' req (ping_id=" << state.request.id
                      << ",   to_node_id=" << remote_node_id_ << ")."
                      << CommonHelpers::Printers::describeDurationInMs(uptime()) << ", Δ "
                      << CommonHelpers::Printers::describeDurationInUs(request_duration) << std::endl;  // NOLINT
        }
    }

    // MARK: Data members:
    // NOLINTBEGIN

    struct State
    {
        Linux::CanMedia::Collection media_collection_;
        CanTransportPtr             transport_;

    };  // State

    TrackingMemoryResource                                mr_;
    example::platform::Linux::EpollSingleThreadedExecutor executor_;
    TimePoint                                             startup_time_{};
    NodeId                                                local_node_id_{42};
    NodeId                                                remote_node_id_{42};
    Duration                                              run_duration_{10s};
    bool                                                  print_activities_{true};
    std::vector<std::string>                              iface_addresses_{"vcan0"};
    // NOLINTEND

};  // Example_1_Presentation_3_HB_GetInfo_Ping_Can

TEST_F(Example_1_Presentation_3_HB_GetInfo_Ping_Can, main)
{
    using PingRequest        = UserService::PingRequest;
    using PingClient         = Client<UserService::PingRequest, UserService::PongResponse>;
    using PingServer         = Server<UserService::PingRequest, UserService::PongResponse>;
    using PongContinuation   = PingServer::OnRequestCallback::Continuation;
    using HeartbeatPublisher = Publisher<uavcan::node::Heartbeat_1_0>;

    State state;

    std::cout << "-----------\n";
    std::cout << "Local  node ID: " << local_node_id_ << "\n";
    std::cout << "Remote node ID: " << remote_node_id_ << "\n";
    std::cout << "Interfaces    : '" << CommonHelpers::joinInterfaceAddresses(iface_addresses_) << "'\n";

    // 1. Make CAN transport with collection of media.
    //
    if (!state.media_collection_.make(executor_, iface_addresses_))
    {
        GTEST_SKIP();
    }
    constexpr std::size_t tx_capacity = 16;
    auto maybe_transport              = makeTransport({mr_}, executor_, state.media_collection_.span(), tx_capacity);
    ASSERT_THAT(maybe_transport, testing::VariantWith<CanTransportPtr>(testing::NotNull()))
        << "Can't create transport.";
    state.transport_ = cetl::get<CanTransportPtr>(std::move(maybe_transport));
    state.transport_->setLocalNodeId(local_node_id_);
    state.transport_->setTransientErrorHandler(CommonHelpers::Can::transientErrorReporter);

    // 2. Create presentation layer object.
    //
    Presentation presentation{mr_, executor_, *state.transport_};

    // 3. Bring up "Ping" server.
    //
    // Fot the sake of demonstration, we will keep track of all "Ping" requests inside the `ping_contexts` map,
    // and respond to them with "Pong" after variable delay, namely after `10ms + 10ms * (ping_id % 3)`.
    // As a result, we will have 3 different delays for 3 different "Ping" requests, which in turn reshuffle the order
    // of incoming responses - useful for testing multiple overlapping/concurrent requests on the same service.
    // It also demonstrates how to use `Continuation` to store it, and reply in async manner (after some delay).
    //
    std::map<std::size_t, std::tuple<PongContinuation, Callback::Any, UserService::PingRequest>> ping_contexts;
    //
    std::size_t unique_request_id = 0;
    auto        maybe_ping_server = presentation.makeServer<UserService::PingRequest,
                                                            UserService::PongResponse>(  //
        UserService::PingRequest::ServiceId,
        [&](const auto& arg, auto continuation) {
            //
            if (print_activities_)
            {
                std::cout << " ◯  Server received     'Ping' req (ping_id=" << arg.request.id
                          << ", from_node_id=" << arg.metadata.remote_node_id << ")."
                          << CommonHelpers::Printers::describeDurationInMs(arg.approx_now - startup_time_)
                          << ", tf_id=" << arg.metadata.rx_meta.base.transfer_id
                          << ", pri=" << static_cast<int>(arg.metadata.rx_meta.base.priority) << std::endl;  // NOLINT
            }
            auto delay_cb = executor_.registerCallback([&ping_contexts, id = unique_request_id](const auto& cb_arg) {
                //
                auto& ping_context = ping_contexts[id];

                UserService::PongResponse response{};
                response.id = std::get<2>(ping_context).id;

                auto& continuation = std::get<0>(ping_context);
                continuation(cb_arg.approx_now + 1s, response);
                ping_contexts.erase(id);
            });
            delay_cb.schedule(Callback::Schedule::Once{arg.approx_now + 1us + (10us * (arg.request.id % 7))});
            ping_contexts[unique_request_id++] =
                std::make_tuple(std::move(continuation), std::move(delay_cb), arg.request);
        });
    ASSERT_THAT(maybe_ping_server, testing::VariantWith<PingServer>(testing::_)) << "Failed to create 'Ping' server.";
    // we don't need the actual server object further - just keep it alive (inside `maybe_ping_server`).

    // 4. Make "Ping" client.
    //
    auto maybe_ping_client = presentation.makeClient<UserService::PingRequest, UserService::PongResponse>(  //
        remote_node_id_,
        UserService::PingRequest::ServiceId);
    ASSERT_THAT(maybe_ping_client, testing::VariantWith<PingClient>(testing::_)) << "Failed to create 'Ping' client.";
    auto ping_client = cetl::get<PingClient>(std::move(maybe_ping_client));

    // 5. Send periodic "Ping" requests, and print "Pong" replies.
    //
    // Fot the sake of demonstration, we will send every second 3 concurrent "Ping" requests with different payloads
    // (the `id` field), which will implicitly affect the order of responses (see server setup).
    //
    constexpr std::size_t       concurrent_requests = 5;
    CommonHelpers::RunningStats ping_pong_stats;
    std::array<PingPongState, concurrent_requests>
        ping_pong_states{PingPongState{mr_, "A", ping_pong_stats, PingRequest{1000}, Priority::Nominal, {}, {}},
                         PingPongState{mr_, "B", ping_pong_stats, PingRequest{2000}, Priority::Nominal, {}, {}},
                         PingPongState{mr_, "C", ping_pong_stats, PingRequest{3000}, Priority::Nominal, {}, {}},
                         PingPongState{mr_, "D", ping_pong_stats, PingRequest{4000}, Priority::Nominal, {}, {}},
                         PingPongState{mr_, "E", ping_pong_stats, PingRequest{5000}, Priority::Nominal, {}, {}}};
    //
    const auto make_ping_request = [this, &ping_client](PingPongState& state) {
        //
        ++state.request.id;
        if (print_activities_)
        {
            std::cout << "➡️  Client '" << state.name << "' sending  'Ping' req (ping_id=" << state.request.id
                      << ",   to_node_id=" << remote_node_id_ << ")."
                      << CommonHelpers::Printers::describeDurationInMs(uptime()) << std::endl;  // NOLINT
        }
        state.req_start = now();
        ping_client.setPriority(state.priority);
        auto maybe_promise = ping_client.request(state.req_start + 300ms, state.request);
        ASSERT_THAT(maybe_promise, testing::VariantWith<PongPromise>(testing::_)) << "Failed to make 'Ping' request.";
        state.promise.emplace(cetl::get<PongPromise>(std::move(maybe_promise)));
    };
    //
    auto request_periodically_cb_ = executor_.registerCallback([&](const auto&) {
        //
        std::cout << "---------------\n";
        for (auto& ping_pong_state : ping_pong_states)
        {
            make_ping_request(ping_pong_state);
            ping_pong_state.promise->setCallback([&](const auto& arg) {
                //
                processPingPongResult(ping_pong_state, arg);
            });
        }
    });
    request_periodically_cb_.schedule(Callback::Schedule::Repeat{startup_time_ + 1s, 3s});

    // 6. Publish heartbeats.
    //
    auto maybe_heartbeat_pub = presentation.makePublisher<uavcan::node::Heartbeat_1_0>();
    ASSERT_THAT(maybe_heartbeat_pub, testing::VariantWith<HeartbeatPublisher>(testing::_))
        << "Can't create 'Heartbeat' publisher.";
    auto heartbeat_pub                 = cetl::get<HeartbeatPublisher>(std::move(maybe_heartbeat_pub));
    auto publish_heartbeat_every_1s_cb = executor_.registerCallback([&](const auto& arg) {
        //
        EXPECT_THAT(heartbeat_pub.publish(arg.approx_now + 1s, makeHeartbeatMsg(arg.approx_now)),
                    testing::Eq(cetl::nullopt));
    });
    //
    constexpr auto hb_period = std::chrono::seconds{uavcan::node::Heartbeat_1_0::MAX_PUBLICATION_PERIOD};
    publish_heartbeat_every_1s_cb.schedule(Callback::Schedule::Repeat{startup_time_ + hb_period, hb_period});

    // 7. Bring up 'GetInfo' server.
    //
    using GetInfo_1_0 = uavcan::node::GetInfo_1_0;
    uavcan::node::GetInfo_1_0::Response get_info_response{{&mr_}};
    get_info_response.protocol_version.major = 1;
    const std::string node_name{"org.opencyphal.Ex_1_Pres_3_HB_GetInfo_Ping_CAN"};
    std::copy_n(node_name.begin(), std::min(node_name.size(), 50UL), std::back_inserter(get_info_response.name));
    //
    auto maybe_get_info_srv = presentation.makeServer<GetInfo_1_0>([&](const auto& arg, auto continuation) {
        //
        std::cout << "ⓘ  Received 'GetInfo' request (from_node_id=" << arg.metadata.remote_node_id << ")."
                  << std::endl;  // NOLINT
        continuation(arg.approx_now + 1s, get_info_response);
    });
    ASSERT_THAT(maybe_get_info_srv, testing::VariantWith<ServiceServer<GetInfo_1_0>>(testing::_))
        << "Can't create 'GetInfo' server.";

    // 8. Main loop.
    //
    Duration        worst_lateness{0};
    const TimePoint deadline = startup_time_ + run_duration_ + 500ms;
    std::cout << "================>\nRunning..." << std::endl;  // NOLINT
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

    std::cout << ">===============|\nDone.\nStats:\n";
    std::cout << "worst_callback_lateness  = " << worst_lateness.count() << " us\n";
    std::cout << "call_duration_stats_mean = " << ping_pong_stats.mean() << " us\n";
    std::cout << "call_duration_stats_std  ± " << ping_pong_stats.standardDeviation() << " us (±"
              << std::setprecision(3) << 100.0 * ping_pong_stats.standardDeviation() / ping_pong_stats.mean() << "%)\n";
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
