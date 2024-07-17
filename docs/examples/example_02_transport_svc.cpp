/// @file
/// Example of creating a libcyphal node in your project.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "platform/posix/udp_media.hpp"

#include "nunavut/support/serialization.hpp"
#include "uavcan/node/Health_1_0.hpp"
#include "uavcan/node/Heartbeat_1_0.hpp"
#include "uavcan/node/Mode_1_0.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/platform/single_threaded_executor.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/udp/media.hpp>
#include <libcyphal/transport/udp/udp_transport.hpp>
#include <libcyphal/transport/udp/udp_transport_impl.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <utility>

namespace
{

using namespace libcyphal::transport;       // NOLINT This our main concern here in this test.
using namespace libcyphal::transport::udp;  // NOLINT This our main concern here in this test.

using CallbackHandle      = libcyphal::IExecutor::Callback::Handle;
using UdpTransportPtr     = libcyphal::UniquePtr<libcyphal::transport::udp::IUdpTransport>;
using MessageTxSessionPtr = libcyphal::UniquePtr<libcyphal::transport::IMessageTxSession>;

using std::literals::chrono_literals::operator""s;
using std::literals::chrono_literals::operator""ms;
using std::literals::chrono_literals::operator""us;

using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::VariantWith;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class Example_02_Transport : public testing::Test
{
protected:
    void TearDown() override {}

    template <std::size_t Redundancy>
    UdpTransportPtr makeUdpTransport(std::array<udp::IMedia*, Redundancy>& media_array, const NodeId local_node_id)
    {
        const std::size_t tx_capacity = 16;

        auto maybe_transport = udp::makeTransport({mr_}, executor_, media_array, tx_capacity);
        EXPECT_THAT(maybe_transport, VariantWith<UdpTransportPtr>(_)) << "Failed to create transport.";
        auto udp_transport = cetl::get<UdpTransportPtr>(std::move(maybe_transport));

        udp_transport->setLocalNodeId(local_node_id);

        return udp_transport;
    }

    uavcan::node::Heartbeat_1_0 makeHeartbeat() const
    {
        auto uptime         = executor_.now().time_since_epoch();
        auto uptime_in_secs = std::chrono::duration_cast<std::chrono::seconds>(uptime);
        return {static_cast<std::uint32_t>(uptime_in_secs.count()),
                {uavcan::node::Health_1_0::NOMINAL},
                {uavcan::node::Mode_1_0::OPERATIONAL}};
    }

    template <typename T, typename TxSession, typename TxMetadata>
    cetl::optional<AnyFailure> serializeAndSend(const T& value, TxSession& tx_session, const TxMetadata& metadata)
    {
        using traits = typename T::_traits_;
        std::array<std::uint8_t, traits::SerializationBufferSizeBytes> buffer{};

        auto result = uavcan::node::serialize(value, buffer);

        // NOLINTNEXTLINE
        const cetl::span<const cetl::byte> fragment{reinterpret_cast<cetl::byte*>(buffer.data()), result.value()};
        const std::array<const cetl::span<const cetl::byte>, 1> payload{fragment};

        return tx_session.send(metadata, payload);
    }

    // MARK: Data members:

    // NOLINTBEGIN
    cetl::pmr::memory_resource&                 mr_{*cetl::pmr::new_delete_resource()};
    libcyphal::platform::SingleThreadedExecutor executor_{mr_};
    // NOLINTEND
};

// MARK: - Tests:

TEST_F(Example_02_Transport, posix_udp)
{
    const libcyphal::transport::NodeId local_node_id{2000};

    // Make UDP transport with a single media.
    //
    example::platform::posix::UdpMedia udp_media{mr_};
    std::array<udp::IMedia*, 1>        media_array{&udp_media};
    auto                               udp_transport = makeUdpTransport(media_array, local_node_id);

    // Make a message tx session for heartbeat port # 7509.
    //
    auto maybe_msg_tx_session = udp_transport->makeMessageTxSession({7509});
    ASSERT_THAT(maybe_msg_tx_session, VariantWith<MessageTxSessionPtr>(_)) << "Failed to create request tx session.";
    auto msg_tx_session = cetl::get<MessageTxSessionPtr>(std::move(maybe_msg_tx_session));
    msg_tx_session->setSendTimeout(1000s);  // for stepping in debugger

    // Publish heartbeat every second.
    //
    TransferId                    heartbeat_transfer_id{0};
    constexpr libcyphal::Duration HeartbeatPeriod = 1s;
    //
    CallbackHandle periodic = executor_.registerCallback([&](const auto now) {
        //
        heartbeat_transfer_id += 1;
        EXPECT_THAT(serializeAndSend(makeHeartbeat(),
                                     *msg_tx_session,
                                     TransferMetadata{heartbeat_transfer_id, now, Priority::Nominal}),
                    Eq(cetl::nullopt));

        periodic.scheduleAt(now + HeartbeatPeriod);
    });
    periodic.scheduleAt(executor_.now());

    // Main loop.
    //
    const auto deadline = executor_.now() + 10s;
    while (executor_.now() < deadline)
    {
        executor_.spinOnce();

        std::this_thread::sleep_for(1ms);
    }
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
