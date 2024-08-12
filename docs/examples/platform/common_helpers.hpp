/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef EXAMPLE_PLATFORM_COMMON_HELPERS_HPP_INCLUDED
#define EXAMPLE_PLATFORM_COMMON_HELPERS_HPP_INCLUDED

#include "posix/posix_single_threaded_executor.hpp"

#include <libcyphal/executor.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/transport.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <cassert>  // NOLINT for NUNAVUT_ASSERT
#include <nunavut/support/serialization.hpp>
#include <uavcan/node/Health_1_0.hpp>
#include <uavcan/node/Heartbeat_1_0.hpp>
#include <uavcan/node/Mode_1_0.hpp>

#include <cetl/pf17/cetlpf.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace example
{
namespace platform
{

struct CommonHelpers
{
    using Callback            = libcyphal::IExecutor::Callback;
    using MessageRxSessionPtr = libcyphal::UniquePtr<libcyphal::transport::IMessageRxSession>;
    using MessageTxSessionPtr = libcyphal::UniquePtr<libcyphal::transport::IMessageTxSession>;

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

    struct Printers
    {
        static std::string describeError(const libcyphal::transport::StateError&)
        {
            return "StateError";
        }
        static std::string describeError(const libcyphal::transport::AnonymousError&)
        {
            return "AnonymousError";
        }
        static std::string describeError(const libcyphal::transport::ArgumentError&)
        {
            return "ArgumentError";
        }
        static std::string describeError(const libcyphal::transport::MemoryError&)
        {
            return "MemoryError";
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

    template <typename T, typename TxSession, typename TxMetadata>
    static cetl::optional<libcyphal::transport::AnyFailure> serializeAndSend(const T&          value,
                                                                             TxSession&        tx_session,
                                                                             const TxMetadata& metadata)
    {
        using traits = typename T::_traits_;
        std::array<std::uint8_t, traits::SerializationBufferSizeBytes> buffer{};

        const auto data_size = uavcan::node::serialize(value, buffer).value();

        // NOLINTNEXTLINE
        const cetl::span<const cetl::byte> fragment{reinterpret_cast<cetl::byte*>(buffer.data()), data_size};
        const std::array<const cetl::span<const cetl::byte>, 1> payload{fragment};

        return tx_session.send(metadata, payload);
    }

    struct Heartbeat
    {
        struct Rx
        {
            libcyphal::TimePoint startup_time_;
            MessageRxSessionPtr  msg_rx_session_;

            bool makeRxSession(libcyphal::transport::ITransport& transport, const libcyphal::TimePoint startup_time)
            {
                auto maybe_msg_rx_session =
                    transport.makeMessageRxSession({uavcan::node::Heartbeat_1_0::_traits_::ExtentBytes,
                                                    uavcan::node::Heartbeat_1_0::_traits_::FixedPortId});
                EXPECT_THAT(maybe_msg_rx_session, testing::VariantWith<MessageRxSessionPtr>(testing::_))
                    << "Failed to create Heartbeat RX session.";
                if (auto* const session = cetl::get_if<MessageRxSessionPtr>(&maybe_msg_rx_session))
                {
                    startup_time_   = startup_time;
                    msg_rx_session_ = std::move(*session);
                }
                return nullptr != msg_rx_session_;
            }

            void reset()
            {
                msg_rx_session_.reset();
            }

            void receive() const
            {
                if (msg_rx_session_)
                {
                    auto rx_heartbeat = msg_rx_session_->receive();
                    if (rx_heartbeat)
                    {
                        print(*rx_heartbeat);
                    }
                }
            }

            void print(const libcyphal::transport::MessageRxTransfer& rx_heartbeat) const
            {
                const auto rel_time = rx_heartbeat.metadata.base.timestamp - startup_time_;
                std::cout << "Received heartbeat from node " << std::setw(8)
                          << rx_heartbeat.metadata.publisher_node_id.value_or(0) << " @ " << std::setw(8)
                          << std::chrono::duration_cast<std::chrono::milliseconds>(rel_time).count()
                          << " ms, tx_id=" << rx_heartbeat.metadata.base.transfer_id << "\n"
                          << std::flush;
            }

        };  // Rx

        struct Tx
        {
            libcyphal::TimePoint             startup_time_;
            libcyphal::transport::TransferId transfer_id_{0};
            MessageTxSessionPtr              msg_tx_session_;
            Callback::Any                    callback_;

            void makeTxSession(libcyphal::transport::ITransport& transport,
                               libcyphal::IExecutor&             executor,
                               const libcyphal::TimePoint        startup_time)
            {
                auto maybe_msg_tx_session =
                    transport.makeMessageTxSession({uavcan::node::Heartbeat_1_0::_traits_::FixedPortId});
                EXPECT_THAT(maybe_msg_tx_session, testing::VariantWith<MessageTxSessionPtr>(testing::_))
                    << "Failed to create Heartbeat TX session.";
                if (auto* const session = cetl::get_if<MessageTxSessionPtr>(&maybe_msg_tx_session))
                {
                    startup_time_   = startup_time;
                    msg_tx_session_ = std::move(*session);

                    callback_             = executor.registerCallback([&](const auto now) { publish(now); });
                    constexpr auto period = std::chrono::seconds{uavcan::node::Heartbeat_1_0::MAX_PUBLICATION_PERIOD};
                    callback_.schedule(Callback::Schedule::Repeat{startup_time + period, period});
                }
            }

            void reset()
            {
                callback_.reset();
                msg_tx_session_.reset();
            }

            void publish(const Callback::Arg& arg)
            {
                transfer_id_ += 1;
                const auto uptime_in_secs =
                    std::chrono::duration_cast<std::chrono::seconds>(arg.approx_now - startup_time_);
                const uavcan::node::Heartbeat_1_0 heartbeat{static_cast<std::uint32_t>(uptime_in_secs.count()),
                                                            {uavcan::node::Health_1_0::NOMINAL},
                                                            {uavcan::node::Mode_1_0::OPERATIONAL}};
                EXPECT_THAT(serializeAndSend(heartbeat,
                                             *msg_tx_session_,
                                             libcyphal::transport::
                                                 TransferMetadata{transfer_id_,
                                                                  arg.approx_now,
                                                                  libcyphal::transport::Priority::Nominal}),
                            testing::Eq(cetl::nullopt))
                    << "Failed to publish heartbeat.";
            }

        };  // Tx

    };  // Heartbeat

    static void runMainLoop(posix::PosixSingleThreadedExecutor& executor,
                            const libcyphal::TimePoint          deadline,
                            const std::function<void()>&        spin_extra_action)
    {
        libcyphal::Duration worst_lateness{0};

        while (executor.now() < deadline)
        {
            const auto spin_result = executor.spinOnce();
            worst_lateness         = std::max(worst_lateness, spin_result.worst_lateness);

            spin_extra_action();

            cetl::optional<libcyphal::Duration> opt_timeout;
            if (spin_result.next_exec_time.has_value())
            {
                opt_timeout = spin_result.next_exec_time.value() - executor.now();
            }
            EXPECT_THAT(executor.pollAwaitableResourcesFor(opt_timeout), testing::Eq(cetl::nullopt));
        }

        std::cout << "worst_callback_lateness=" << worst_lateness.count() << "us\n";
    }

};  // CommonHelpers

}  // namespace platform
}  // namespace example

#endif  // EXAMPLE_PLATFORM_TRACKING_MEMORY_RESOURCE_HPP_INCLUDED
