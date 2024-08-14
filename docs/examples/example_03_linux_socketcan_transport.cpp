/// @file
/// Example of creating a libcyphal node in your project using posix SOCKETCAN media and CAN transport.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "platform/common_helpers.hpp"
#include "platform/posix/posix_single_threaded_executor.hpp"
#include "platform/tracking_memory_resource.hpp"

#ifdef __linux__
#    include "platform/linux/can/can_media.hpp"
#endif

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>
#include <cetl/visit_helpers.hpp>
#include <libcyphal/transport/can/can_transport.hpp>
#include <libcyphal/transport/can/can_transport_impl.hpp>
#include <libcyphal/transport/can/media.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
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

class Example_03_LinuxSocketCanTransport : public testing::Test
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

    cetl::optional<AnyFailure> transientErrorReporter(ICanTransport::TransientErrorReport::Variant& report_var)
    {
        using Report = ICanTransport::TransientErrorReport;

        cetl::visit(  //
            cetl::make_overloaded(
                [](const Report::CanardTxPush& report) {
                    std::cerr << "Failed to push TX frame to canard "
                              << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                              << CommonHelpers::Printers::describeAnyFailure(report.failure) << "\n";
                },
                [](const Report::CanardRxAccept& report) {
                    std::cerr << "Failed to accept RX frame at canard "
                              << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                              << CommonHelpers::Printers::describeAnyFailure(report.failure) << "\n";
                },
                [](const Report::MediaPop& report) {
                    std::cerr << "Failed to pop frame from media "
                              << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                              << CommonHelpers::Printers::describeAnyFailure(report.failure) << "\n";
                },
                [](const Report::ConfigureMedia& report) {
                    std::cerr << "Failed to configure CAN.\n"
                              << CommonHelpers::Printers::describeAnyFailure(report.failure) << "\n";
                },
                [](const Report::MediaConfig& report) {
                    std::cerr << "Failed to configure media "
                              << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                              << CommonHelpers::Printers::describeAnyFailure(report.failure) << "\n";
                },
                [this](const Report::MediaPush& report) {
                    std::cerr << "Failed to push frame to media "
                              << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                              << CommonHelpers::Printers::describeAnyFailure(report.failure) << "\n";

                    state_.media_vector_[report.media_index].tryReopen();
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

        auto maybe_transport = can::makeTransport({mr_}, executor_, {mis.data(), mis.size()}, tx_capacity);
        EXPECT_THAT(maybe_transport, VariantWith<CanTransportPtr>(_)) << "Failed to create transport.";

        state_.transport_ = cetl::get<CanTransportPtr>(std::move(maybe_transport));
        state_.transport_->setLocalNodeId(local_node_id_);
        state_.transport_->setTransientErrorHandler([this](auto& report) {
            //
            return transientErrorReporter(report);
        });
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
        CanTransportPtr              transport_;
        std::vector<Linux::CanMedia> media_vector_;

    };  // State

    example::platform::TrackingMemoryResource             mr_;
    example::platform::posix::PosixSingleThreadedExecutor executor_{mr_};
    State                                                 state_{};
    NodeId                                                local_node_id_{42};
    TimePoint                                             startup_time_{};
    Duration                                              run_duration_{10s};
    std::vector<std::string>                              iface_addresses_{"vcan0"};
    // NOLINTEND

};  // Example_03_LinuxSocketCanTransport

// MARK: - Tests:

TEST_F(Example_03_LinuxSocketCanTransport, heartbeat)
{
    // Make CAN media.
    //
    for (const auto& iface_address : iface_addresses_)
    {
        auto maybe_media = Linux::CanMedia::make(executor_, iface_address);
        if (auto* const error = cetl::get_if<PlatformError>(&maybe_media))
        {
            std::cerr << "Failed to create CAN media '" << iface_address << "', errno=" << (*error)->code() << ".";
            GTEST_SKIP();
        }
        ASSERT_THAT(maybe_media, VariantWith<Linux::CanMedia>(_)) << "Failed to create CAN media.";
        state_.media_vector_.emplace_back(cetl::get<Linux::CanMedia>(std::move(maybe_media)));
    }

    // Make CAN transport with collection of media.
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
