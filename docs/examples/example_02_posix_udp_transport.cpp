/// @file
/// Example of creating a libcyphal node in your project using posix UDP sockets and transport.
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
#include <cetl/pf20/cetlpf.hpp>
#include <cetl/visit_helpers.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/udp/media.hpp>
#include <libcyphal/transport/udp/udp_transport.hpp>
#include <libcyphal/transport/udp/udp_transport_impl.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

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
using namespace libcyphal::transport;       // NOLINT This our main concern here in this test.
using namespace libcyphal::transport::udp;  // NOLINT This our main concern here in this test.

using Duration        = libcyphal::Duration;
using TimePoint       = libcyphal::TimePoint;
using UdpTransportPtr = libcyphal::UniquePtr<IUdpTransport>;

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

class Example_02_PosixUdpTransport : public testing::Test
{
protected:
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
        EXPECT_THAT(mr_.allocated_bytes, 0);
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    static cetl::optional<AnyFailure> transientErrorReporter(IUdpTransport::TransientErrorReport::Variant& report_var)
    {
        using Report = IUdpTransport::TransientErrorReport;

        cetl::visit(  //
            cetl::make_overloaded(
                [](const Report::UdpardTxPublish& report) {
                    std::cerr << "Failed to TX message frame to udpard "
                              << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                              << CommonHelpers::Printers::describeAnyFailure(report.failure) << "\n";
                },
                [](const Report::UdpardTxRequest& report) {
                    std::cerr << "Failed to TX request frame to udpard "
                              << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                              << CommonHelpers::Printers::describeAnyFailure(report.failure) << "\n";
                },
                [](const Report::UdpardTxRespond& report) {
                    std::cerr << "Failed to TX response frame to udpard "
                              << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                              << CommonHelpers::Printers::describeAnyFailure(report.failure) << "\n";
                },
                [](const Report::UdpardRxMsgReceive& report) {
                    std::cerr << "Failed to accept RX message frame at udpard "
                              << CommonHelpers::Printers::describeAnyFailure(report.failure) << "\n";
                },
                [](const Report::UdpardRxSvcReceive& report) {
                    std::cerr << "Failed to accept RX service frame at udpard "
                              << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                              << CommonHelpers::Printers::describeAnyFailure(report.failure) << "\n";
                },
                [](const Report::MediaMakeRxSocket& report) {
                    std::cerr << "Failed to make RX socket " << "(mediaIdx=" << static_cast<int>(report.media_index)
                              << ").\n"
                              << CommonHelpers::Printers::describeAnyFailure(report.failure) << "\n";
                },
                [](const Report::MediaMakeTxSocket& report) {
                    std::cerr << "Failed to make TX socket " << "(mediaIdx=" << static_cast<int>(report.media_index)
                              << ").\n"
                              << CommonHelpers::Printers::describeAnyFailure(report.failure) << "\n";
                },
                [](const Report::MediaTxSocketSend& report) {
                    std::cerr << "Failed to TX frame to socket " << "(mediaIdx=" << static_cast<int>(report.media_index)
                              << ").\n"
                              << CommonHelpers::Printers::describeAnyFailure(report.failure) << "\n";
                },
                [](const Report::MediaRxSocketReceive& report) {
                    std::cerr << "Failed to RX frame from socket "
                              << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                              << CommonHelpers::Printers::describeAnyFailure(report.failure) << "\n";
                }),
            report_var);

        return cetl::nullopt;
    }

    void makeTransport()
    {
        constexpr std::size_t tx_capacity = 16;

        std::vector<IMedia*> mis;
        for (auto& media : state_.media_vector_)
        {
            mis.push_back(&media);
        }

        auto maybe_transport = udp::makeTransport({mr_}, executor_, {mis.data(), mis.size()}, tx_capacity);
        EXPECT_THAT(maybe_transport, VariantWith<UdpTransportPtr>(_)) << "Failed to create transport.";
        state_.transport_ = cetl::get<UdpTransportPtr>(std::move(maybe_transport));
        state_.transport_->setLocalNodeId(local_node_id_);
        state_.transport_->setTransientErrorHandler(transientErrorReporter);
    }

    // MARK: Data members:
    // NOLINTBEGIN

    struct State
    {
        void reset()
        {
            rx_heartbeat_.reset();
            tx_heartbeat_.reset();
            transport_.reset();
            media_vector_.clear();
        }

        CommonHelpers::Heartbeat::Rx rx_heartbeat_;
        CommonHelpers::Heartbeat::Tx tx_heartbeat_;
        UdpTransportPtr              transport_;
        std::vector<posix::UdpMedia> media_vector_;

    };  // State

    TrackingMemoryResource             mr_;
    posix::PosixSingleThreadedExecutor executor_{mr_};
    State                              state_{};
    NodeId                             local_node_id_{42};
    TimePoint                          startup_time_{};
    Duration                           run_duration_{10s};
    std::vector<std::string>           iface_addresses_{"127.0.0.1"};
    // NOLINTEND

};  // Example_02_PosixUdpTransport

// MARK: - Tests:

TEST_F(Example_02_PosixUdpTransport, heartbeat)
{
    // Make UDP media.
    //
    for (const auto& iface_address : iface_addresses_)
    {
        state_.media_vector_.emplace_back(mr_, executor_, iface_address);
    }

    // Make UDP transport with collection of media.
    makeTransport();

    // Subscribe/Publish heartbeats.
    state_.rx_heartbeat_.makeRxSession(*state_.transport_, startup_time_);
    state_.tx_heartbeat_.makeTxSession(*state_.transport_, executor_, startup_time_);

    // Main loop.
    //
    CommonHelpers::runMainLoop(executor_, startup_time_ + run_duration_ + 500ms, [&] {
        //
        state_.rx_heartbeat_.receive();
    });

    state_.reset();
    executor_.releaseTemporaryResources();
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
