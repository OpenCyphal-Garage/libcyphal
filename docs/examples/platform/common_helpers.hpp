/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef EXAMPLE_PLATFORM_COMMON_HELPERS_HPP_INCLUDED
#define EXAMPLE_PLATFORM_COMMON_HELPERS_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/visit_helpers.hpp>
#include <libcyphal/errors.hpp>
#include <libcyphal/transport/can/can_transport.hpp>
#include <libcyphal/transport/can/can_transport_impl.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/udp/udp_transport.hpp>
#include <libcyphal/transport/udp/udp_transport_impl.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace example
{
namespace platform
{

struct CommonHelpers
{
    using NodeId = libcyphal::transport::NodeId;

    static std::vector<std::string> splitInterfaceAddresses(const std::string& iface_addresses_str)
    {
        std::vector<std::string> iface_addresses;
        std::istringstream       iss(iface_addresses_str);
        std::string              str;
        while (std::getline(iss, str, ' '))
        {
            iface_addresses.push_back(str);
        }
        return iface_addresses;
    }

    static std::string joinInterfaceAddresses(const std::vector<std::string>& iface_addresses)  // NOLINT
    {
        // join the interface addresses into a single string using algorithms.
        std::ostringstream oss;
        std::copy(iface_addresses.begin(), iface_addresses.end(), std::ostream_iterator<std::string>(oss, " "));
        const auto str = oss.str();
        return str.substr(0, str.size() - 1);
    }

    struct Printers
    {
        static std::string describeDurationInMs(const libcyphal::Duration& duration)
        {
            std::stringstream ss;
            ss << "   @ " << std::setw(8) << std::right
               << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() << " ms";
            return ss.str();
        }

        static std::string describeDurationInUs(const libcyphal::Duration& duration)
        {
            std::stringstream ss;
            ss << std::chrono::duration_cast<std::chrono::microseconds>(duration).count() << " us";
            return ss.str();
        }

        static std::string describeError(const libcyphal::ArgumentError&)
        {
            return "ArgumentError";
        }
        static std::string describeError(const libcyphal::MemoryError&)
        {
            return "MemoryError";
        }
        static std::string describeError(const libcyphal::transport::AnonymousError&)
        {
            return "AnonymousError";
        }
        static std::string describeError(const libcyphal::transport::CapacityError&)
        {
            return "CapacityError";
        }
        static std::string describeError(const libcyphal::transport::AlreadyExistsError&)
        {
            return "AlreadyExistsError";
        }
        static std::string describeError(const libcyphal::transport::PlatformError& error)
        {
            const auto code = error->code();
            return "Failure: PlatformError{code=" + std::to_string(code) + ", msg='" +
                   std::strerror(static_cast<int>(code)) + "'}";
        }

        static std::string describeAnyFailure(const libcyphal::transport::AnyFailure& failure)
        {
            return "Failure: " + cetl::visit([](const auto& error) { return describeError(error); }, failure);
        }
    };

    template <typename Executor>
    static void runMainLoop(Executor&                                        executor,
                            const libcyphal::TimePoint                       deadline,
                            const std::function<void(libcyphal::TimePoint)>& spin_extra_action)
    {
        using std::literals::chrono_literals::operator""s;

        libcyphal::Duration worst_lateness{0};
        std::cout << "-----------\nRunning..." << std::endl;  // NOLINT

        while (executor.now() < deadline)
        {
            const auto spin_result = executor.spinOnce();
            worst_lateness         = std::max(worst_lateness, spin_result.worst_lateness);

            spin_extra_action(spin_result.approx_now);

            cetl::optional<libcyphal::Duration> opt_timeout{1s};  // awake at least once per second
            if (spin_result.next_exec_time.has_value())
            {
                opt_timeout = std::min(*opt_timeout, spin_result.next_exec_time.value() - executor.now());
            }
            EXPECT_THAT(executor.pollAwaitableResourcesFor(opt_timeout), testing::Eq(cetl::nullopt));
        }

        std::cout << "Done.\n-----------\nStats:\n";
        std::cout << "worst_callback_lateness=" << worst_lateness.count() << "us\n";
    }

    struct Can
    {
        using ICanTransport   = libcyphal::transport::can::ICanTransport;
        using CanTransportPtr = libcyphal::UniquePtr<ICanTransport>;
        using IMedia          = libcyphal::transport::can::IMedia;

        template <typename State>
        static void makeTransport(State&                      state,
                                  cetl::pmr::memory_resource& mr,
                                  libcyphal::IExecutor&       executor,
                                  const NodeId                local_node_id_)
        {
            constexpr std::size_t tx_capacity = 16;

            // Make CAN transport.
            //
            auto maybe_transport = libcyphal::transport::can::makeTransport(  //
                {mr},
                executor,
                state.media_collection_.span(),
                tx_capacity);
            EXPECT_THAT(maybe_transport, testing::VariantWith<CanTransportPtr>(testing::NotNull()))
                << "Failed to create CAN transport.";
            state.transport_ = cetl::get<CanTransportPtr>(std::move(maybe_transport));
            state.transport_->setLocalNodeId(local_node_id_);
            state.transport_->setTransientErrorHandler(transientErrorReporter);
        }

        static cetl::optional<libcyphal::transport::AnyFailure> transientErrorReporter(
            libcyphal::transport::can::ICanTransport::TransientErrorReport::Variant& report_var)
        {
            using Report = libcyphal::transport::can::ICanTransport::TransientErrorReport;

            cetl::visit(  //
                cetl::make_overloaded(
                    [](const Report::CanardTxPush& report) {
                        std::cerr << "Failed to push TX frame to canard "
                                  << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                                  << Printers::describeAnyFailure(report.failure) << "\n";
                    },
                    [](const Report::CanardRxAccept& report) {
                        std::cerr << "Failed to accept RX frame at canard "
                                  << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                                  << Printers::describeAnyFailure(report.failure) << "\n";
                    },
                    [](const Report::MediaPop& report) {
                        std::cerr << "Failed to pop frame from media "
                                  << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                                  << Printers::describeAnyFailure(report.failure) << "\n";
                    },
                    [](const Report::ConfigureMedia& report) {
                        std::cerr << "Failed to configure CAN.\n"
                                  << Printers::describeAnyFailure(report.failure) << "\n";
                    },
                    [](const Report::MediaConfig& report) {
                        std::cerr << "Failed to configure media "
                                  << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                                  << Printers::describeAnyFailure(report.failure) << "\n";
                    },
                    [](const Report::MediaPush& report) {
                        std::cerr << "Failed to push frame to media "
                                  << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                                  << Printers::describeAnyFailure(report.failure) << "\n";
                    }),
                report_var);

            return cetl::nullopt;
        }

    };  // Can

    struct Udp
    {
        using IUdpTransport   = libcyphal::transport::udp::IUdpTransport;
        using UdpTransportPtr = libcyphal::UniquePtr<IUdpTransport>;
        using IMedia          = libcyphal::transport::udp::IMedia;

        template <typename State>
        static void makeTransport(State&                      state,
                                  cetl::pmr::memory_resource& mr,
                                  libcyphal::IExecutor&       executor,
                                  const NodeId                local_node_id)
        {
            constexpr std::size_t tx_capacity = 16;

            // Make UDP transport.
            //
            auto maybe_transport = libcyphal::transport::udp::makeTransport(  //
                {mr},
                executor,
                state.media_collection_.span(),
                tx_capacity);
            EXPECT_THAT(maybe_transport, testing::VariantWith<UdpTransportPtr>(testing::NotNull()))
                << "Failed to create transport.";
            state.transport_ = cetl::get<UdpTransportPtr>(std::move(maybe_transport));
            state.transport_->setLocalNodeId(local_node_id);
            state.transport_->setTransientErrorHandler(transientErrorReporter);
        }

        static cetl::optional<libcyphal::transport::AnyFailure> transientErrorReporter(
            IUdpTransport::TransientErrorReport::Variant& report_var)
        {
            using Report = IUdpTransport::TransientErrorReport;

            cetl::visit(  //
                cetl::make_overloaded(
                    [](const Report::UdpardTxPublish& report) {
                        std::cerr << "Failed to TX message frame to udpard "
                                  << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                                  << Printers::describeAnyFailure(report.failure) << "\n";
                    },
                    [](const Report::UdpardTxRequest& report) {
                        std::cerr << "Failed to TX request frame to udpard "
                                  << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                                  << Printers::describeAnyFailure(report.failure) << "\n";
                    },
                    [](const Report::UdpardTxRespond& report) {
                        std::cerr << "Failed to TX response frame to udpard "
                                  << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                                  << Printers::describeAnyFailure(report.failure) << "\n";
                    },
                    [](const Report::UdpardRxMsgReceive& report) {
                        std::cerr << "Failed to accept RX message frame at udpard "
                                  << Printers::describeAnyFailure(report.failure) << "\n";
                    },
                    [](const Report::UdpardRxSvcReceive& report) {
                        std::cerr << "Failed to accept RX service frame at udpard "
                                  << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                                  << Printers::describeAnyFailure(report.failure) << "\n";
                    },
                    [](const Report::MediaMakeRxSocket& report) {
                        std::cerr << "Failed to make RX socket " << "(mediaIdx=" << static_cast<int>(report.media_index)
                                  << ").\n"
                                  << Printers::describeAnyFailure(report.failure) << "\n";
                    },
                    [](const Report::MediaMakeTxSocket& report) {
                        std::cerr << "Failed to make TX socket " << "(mediaIdx=" << static_cast<int>(report.media_index)
                                  << ").\n"
                                  << Printers::describeAnyFailure(report.failure) << "\n";
                    },
                    [](const Report::MediaTxSocketSend& report) {
                        std::cerr << "Failed to TX frame to socket "
                                  << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                                  << Printers::describeAnyFailure(report.failure) << "\n";
                    },
                    [](const Report::MediaRxSocketReceive& report) {
                        std::cerr << "Failed to RX frame from socket "
                                  << "(mediaIdx=" << static_cast<int>(report.media_index) << ").\n"
                                  << Printers::describeAnyFailure(report.failure) << "\n";
                    }),
                report_var);

            return cetl::nullopt;
        }

    };  // Udp

    class RunningStats final
    {
    public:
        void append(const double item, const double weight = 1.0)
        {
            ++total_number;
            if (total_number == 1)
            {
                running_mean                 = item;
                total_weight                 = weight;
                sum_of_square_devs_from_mean = 0.0;
            }
            else
            {
                const double delta            = item - running_mean;
                const double new_total_weight = total_weight + weight;
                sum_of_square_devs_from_mean += total_weight * weight * delta * delta / new_total_weight;
                running_mean += delta * weight / new_total_weight;
                total_weight = new_total_weight;
            }
        }

        double variance() const
        {
            if (total_number == 0)
            {
                return not_a_number;
            }

            return sum_of_square_devs_from_mean / static_cast<double>(total_weight);
        }

        double standardDeviation() const
        {
            return sqrt(variance());
        }

        double mean() const
        {
            return running_mean;
        }

    private:
        static constexpr auto not_a_number{std::numeric_limits<double>::quiet_NaN()};

        std::size_t total_number{0};
        double      total_weight{0.0};
        double      running_mean{not_a_number};
        double      sum_of_square_devs_from_mean{not_a_number};

    };  // RunningStats

};  // CommonHelpers

}  // namespace platform
}  // namespace example

#endif  // EXAMPLE_PLATFORM_TRACKING_MEMORY_RESOURCE_HPP_INCLUDED
