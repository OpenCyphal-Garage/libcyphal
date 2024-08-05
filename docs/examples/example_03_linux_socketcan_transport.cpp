/// @file
/// Example of creating a libcyphal node in your project using posix SOCKETCAN media and CAN transport.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "platform/posix/posix_single_threaded_executor.hpp"
#include "platform/tracking_memory_resource.hpp"

#ifdef __linux__
#    include "platform/linux/can/can_media.hpp"
#endif

#include <cassert>  // NOLINT for NUNAVUT_ASSERT
#include <nunavut/support/serialization.hpp>
#include <uavcan/node/Health_1_0.hpp>
#include <uavcan/node/Heartbeat_1_0.hpp>
#include <uavcan/node/Mode_1_0.hpp>

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/transport.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/can/media.hpp>
#include <libcyphal/transport/can/can_transport.hpp>
#include <libcyphal/transport/can/can_transport_impl.hpp>
#include <libcyphal/types.hpp>

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
#include <locale>
#include <string>
#include <utility>

namespace
{

using namespace libcyphal::transport;       // NOLINT This our main concern here in this test.
using namespace libcyphal::transport::can;  // NOLINT This our main concern here in this test.

using Duration            = libcyphal::Duration;
using TimePoint           = libcyphal::TimePoint;
using Callback            = libcyphal::IExecutor::Callback::Any;
using CanTransportPtr     = libcyphal::UniquePtr<ICanTransport>;
using MessageRxSessionPtr = libcyphal::UniquePtr<IMessageRxSession>;
using MessageTxSessionPtr = libcyphal::UniquePtr<IMessageTxSession>;

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
        std::cerr.imbue(std::locale("en_US.UTF-8"));
        std::cout.imbue(std::locale("en_US.UTF-8"));

        if (const auto* const node_id_str = std::getenv("CYPHAL__NODE__ID"))
        {
            local_node_id_ = static_cast<NodeId>(std::stoul(node_id_str));
        }
        if (const auto* const iface_address_str = std::getenv("CYPHAL__CAN__IFACE"))
        {
            iface_address_ = iface_address_str;
        }

        startup_time_ = executor_.now();
    }

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocated_bytes, 0);
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    template <std::size_t Redundancy>
    CanTransportPtr makeCanTransport(std::array<can::IMedia*, Redundancy>& media_array, const NodeId local_node_id)
    {
        const std::size_t tx_capacity = 16;

        auto maybe_transport = can::makeTransport({mr_}, executor_, media_array, tx_capacity);
        EXPECT_THAT(maybe_transport, VariantWith<CanTransportPtr>(_)) << "Failed to create transport.";
        auto can_transport = cetl::get<CanTransportPtr>(std::move(maybe_transport));
        if (can_transport)
        {
            can_transport->setLocalNodeId(local_node_id);
        }
        return can_transport;
    }

    template <typename T, typename TxSession, typename TxMetadata>
    static cetl::optional<AnyFailure> serializeAndSend(const T&          value,
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

    void publishHeartbeat(const TimePoint now)
    {
        state_.tx_heartbeat_.transfer_id_ += 1;

        const auto uptime         = now.time_since_epoch();
        const auto uptime_in_secs = std::chrono::duration_cast<std::chrono::seconds>(uptime);

        const uavcan::node::Heartbeat_1_0 heartbeat{static_cast<std::uint32_t>(uptime_in_secs.count()),
                                                    {uavcan::node::Health_1_0::NOMINAL},
                                                    {uavcan::node::Mode_1_0::OPERATIONAL}};

        EXPECT_THAT(serializeAndSend(heartbeat,
                                     *state_.tx_heartbeat_.msg_tx_session_,
                                     TransferMetadata{state_.tx_heartbeat_.transfer_id_, now, Priority::Nominal}),
                    Eq(cetl::nullopt))
            << "Failed to publish heartbeat.";
    }
    void printHeartbeat(const MessageRxTransfer& rx_heartbeat)
    {
        const auto rel_time = rx_heartbeat.metadata.base.timestamp - startup_time_;
        std::cerr << "Received heartbeat from node " << rx_heartbeat.metadata.publisher_node_id.value_or(0) << " @ "
                  << std::setw(8) << std::chrono::duration_cast<std::chrono::milliseconds>(rel_time).count()
                  << " ms, tx_id=" << rx_heartbeat.metadata.base.transfer_id << "\n";
    }

    // MARK: Data members:
    // NOLINTBEGIN

    struct State
    {
        struct RxHeartbeat
        {
            MessageRxSessionPtr msg_rx_session_;

            bool makeRxSession(ITransport& transport)
            {
                auto maybe_msg_rx_session =
                    transport.makeMessageRxSession({uavcan::node::Heartbeat_1_0::_traits_::ExtentBytes,
                                                    uavcan::node::Heartbeat_1_0::_traits_::FixedPortId});
                EXPECT_THAT(maybe_msg_rx_session, VariantWith<MessageRxSessionPtr>(_))
                    << "Failed to create Heartbeat RX session.";
                if (auto* const session = cetl::get_if<MessageRxSessionPtr>(&maybe_msg_rx_session))
                {
                    msg_rx_session_ = std::move(*session);
                }
                return nullptr != msg_rx_session_;
            }

            void reset()
            {
                msg_rx_session_.reset();
            }

        };  // RxHeartbeat

        struct TxHeartbeat
        {
            TransferId          transfer_id_{0};
            MessageTxSessionPtr msg_tx_session_;
            Callback            callback_;

            bool makeTxSession(ITransport& transport)
            {
                auto maybe_msg_tx_session =
                    transport.makeMessageTxSession({uavcan::node::Heartbeat_1_0::_traits_::FixedPortId});
                EXPECT_THAT(maybe_msg_tx_session, VariantWith<MessageTxSessionPtr>(_))
                    << "Failed to create Heartbeat TX session.";
                if (auto* const session = cetl::get_if<MessageTxSessionPtr>(&maybe_msg_tx_session))
                {
                    msg_tx_session_ = std::move(*session);
                }
                return nullptr != msg_tx_session_;
            }

            void reset()
            {
                callback_.reset();
                msg_tx_session_.reset();
            }

        };  // TxHeartbeat

        void reset()
        {
            rx_heartbeat_.reset();
            tx_heartbeat_.reset();
        }

        RxHeartbeat rx_heartbeat_;
        TxHeartbeat tx_heartbeat_;

    };  // State

    example::platform::TrackingMemoryResource             mr_;
    example::platform::posix::PosixSingleThreadedExecutor executor_{mr_};
    State                                                 state_{};
    NodeId                                                local_node_id_{42};
    TimePoint                                             startup_time_{};
    std::string                                           iface_address_{"vcan0"};
    // NOLINTEND

};  // Example_03_LinuxSocketCanTransport

// MARK: - Tests:

TEST_F(Example_03_LinuxSocketCanTransport, heartbeat)
{
    using Schedule = libcyphal::IExecutor::Callback::Schedule;

    // Make CAN media.
    //
    using CanMedia   = example::platform::Linux::CanMedia;
    auto maybe_media = CanMedia::make(mr_, executor_, iface_address_);
    if (auto* const error = cetl::get_if<PlatformError>(&maybe_media))
    {
        GTEST_SKIP() << "Failed to create CAN media '"<< iface_address_ << "', errno=" << (*error)->code() << ".";
    }
    ASSERT_THAT(maybe_media, VariantWith<CanMedia>(_)) << "Failed to create CAN media.";
    auto can_media = cetl::get<CanMedia>(std::move(maybe_media));

    // Make CAN transport with a single media.
    //
    std::array<can::IMedia*, 1> media_array{&can_media};
    auto                        can_transport = makeCanTransport(media_array, local_node_id_);

    // Subscribe for heartbeat messages.
    //
    if (!state_.rx_heartbeat_.makeRxSession(*can_transport))
    {
        FAIL() << "Failed to create Heartbeat RX session, can't continue.";
        // unreachable due to FAIL above
    }

    // Publish heartbeat periodically.
    //
    if (state_.tx_heartbeat_.makeTxSession(*can_transport))
    {
        // state_.heartbeat_.msg_tx_session_->setSendTimeout(1000s);  // for stepping in debugger

        state_.tx_heartbeat_.callback_ = executor_.registerCallback([&](const auto now) {
            //
            publishHeartbeat(now);
        });
        constexpr auto period          = std::chrono::seconds{uavcan::node::Heartbeat_1_0::MAX_PUBLICATION_PERIOD};
        executor_.scheduleCallback(state_.tx_heartbeat_.callback_, Schedule::Repeat{startup_time_ + period, period});
    }

    // Main loop.
    //
    Duration   worst_lateness{0};
    const auto deadline = startup_time_ + 20s;
    //
    while (executor_.now() < deadline)
    {
        const auto spin_result = executor_.spinOnce();

        worst_lateness = std::max(worst_lateness, spin_result.worst_lateness);

        if (state_.rx_heartbeat_.msg_rx_session_)
        {
            auto rx_heartbeat = state_.rx_heartbeat_.msg_rx_session_->receive();
            if (rx_heartbeat)
            {
                printHeartbeat(*rx_heartbeat);
            }
        }

        cetl::optional<Duration> opt_timeout;
        if (spin_result.next_exec_time.has_value())
        {
            opt_timeout = spin_result.next_exec_time.value() - executor_.now();
        }
        EXPECT_THAT(executor_.pollAwaitableResourcesFor(opt_timeout), Eq(cetl::nullopt));
    }

    std::cout << "worst_lateness = " << worst_lateness.count() << " us\n";

    state_.reset();
    can_transport.reset();
    executor_.releaseTemporaryResources();
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
