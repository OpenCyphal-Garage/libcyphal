/// @file
/// Main entry point for the demonstration application.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include <iostream>
#include <utility>

#include "cetl/pf17/sys/memory_resource.hpp"

#include "libcyphal/janky.hpp"
#include "libcyphal/libcyphal.hpp"
#include "libcyphal/network/ip/udp.hpp"
#include "libcyphal/network/context.hpp"
#include "libcyphal/transport/udp/transport.hpp"
#include "libcyphal/transport/session.hpp"
#include "libcyphal/transport/data_specifier.hpp"

#include "libcyphal/network/posix/context.hpp"

#include "uavcan/node/Heartbeat_1_0.hpp"
#include "uavcan/node/GetInfo_1_0.hpp"

libcyphal::network::posix::NetworkMemoryResources network_memory{
    cetl::pf17::pmr::new_delete_resource(),
    cetl::pf17::pmr::new_delete_resource(),
    cetl::pf17::pmr::new_delete_resource(),

};

libcyphal::transport::udp::TransportMemoryResources transport_memory{cetl::pf17::pmr::new_delete_resource(),
                                                                     cetl::pf17::pmr::new_delete_resource(),
                                                                     cetl::pf17::pmr::new_delete_resource(),
                                                                     cetl::pf17::pmr::new_delete_resource()};

int main()
{
    // +-----------------------------------------------------------------------+
    // | NETWORK LAYER
    // +-----------------------------------------------------------------------+

    libcyphal::network::posix::Context network_context{network_memory,
                                                       {},
                                                       {libcyphal::network::ip::Address{127, 0, 0, 1},
                                                        libcyphal::network::ip::Address{127, 0, 0, 1}}};

    libcyphal::network::InterfaceEnumerator<libcyphal::network::ip::Address>& ips =
        network_context.enumerateIPInterfaces();
    for (auto ip : ips)
    {
        std::cout << "IP: " << static_cast<std::uint32_t>(ip) << std::endl;
        auto&& socket_perhaps = network_context.makeBoundUdpMulticastOutputSocket(ip);
        if (!socket_perhaps.has_value())
        {
            std::cout << "Failed to make socket for IP: " << static_cast<std::uint32_t>(ip) << std::endl;
            break;
        }
        auto&& socket = socket_perhaps.value();
        if (socket->connect(libcyphal::network::ip::Address{127, 0, 0, 1}, libcyphal::network::ip::udp::CyphalPort))
        {
            std::cout << "Made a connection." << std::endl;
        }
        socket.reset();
    }
    // +-----------------------------------------------------------------------+
    // | TRANSPORT LAYER
    // +-----------------------------------------------------------------------+

    libcyphal::transport::udp::Transport udp{network_context,
                                             static_cast<libcyphal::NodeID>(127U),
                                             std::move(transport_memory)};

    if (!udp.initialize())
    {
        return 1;
    }
    std::cout << "UDP Transport initialized successfully" << std::endl;

    // +-----------------------------------------------------------------------+
    // | OUTPUT :: HEARTBEAT
    // +-----------------------------------------------------------------------+
    libcyphal::janky::optional<libcyphal::transport::DataSpecifier> heartbeat =
        libcyphal::transport::DataSpecifier::message(uavcan::node::Heartbeat_1_0::_traits_::FixedPortId);
    if (!heartbeat)
    {
        return 1;
    }
    std::cout << "Heartbeat specifier created successfully" << std::endl;

    libcyphal::transport::OutputSessionSpecifier output_specifier{*heartbeat};
    libcyphal::transport::PayloadMetadata        md_out{uavcan::node::Heartbeat_1_0::_traits_::ExtentBytes};
    libcyphal::transport::IOutputSession*        out;
    if (!udp.getOutputSession(output_specifier, md_out, out))
    {
        return 1;
    }
    std::cout << "Output session created successfully" << std::endl;

    // +-----------------------------------------------------------------------+
    // | INPUT :: GET_INFO
    // +-----------------------------------------------------------------------+
    libcyphal::janky::optional<libcyphal::transport::DataSpecifier> getinfo =
        libcyphal::transport::DataSpecifier::serviceProvider(uavcan::node::GetInfo_1_0::Request::_traits_::FixedPortId);
    if (!getinfo)
    {
        return 1;
    }
    std::cout << "GetInfo specifier created successfully" << std::endl;

    libcyphal::transport::InputSessionSpecifier input_specifier{*getinfo};
    libcyphal::transport::PayloadMetadata       md_in{uavcan::node::GetInfo_1_0::Request::_traits_::ExtentBytes};
    libcyphal::transport::IInputSession*        in;
    if (!udp.getInputSession(input_specifier, md_in, in))
    {
        return 1;
    }
    std::cout << "Input session created successfully" << std::endl;
    // +-----------------------------------------------------------------------+
    // | SEND :: HEARTBEAT
    // +-----------------------------------------------------------------------+
    uavcan::node::Heartbeat_1_0 heartbeat_message;
    heartbeat_message.health.value                = uavcan::node::Health_1_0::WARNING;
    heartbeat_message.mode.value                  = uavcan::node::Mode_1_0::INITIALIZATION;
    heartbeat_message.vendor_specific_status_code = 0;
    heartbeat_message.uptime                      = 0;

    cetl::pf17::byte buffer[uavcan::node::Heartbeat_1_0::_traits_::ExtentBytes];
    static_assert(sizeof(std::uint8_t) == sizeof(cetl::pf17::byte), "Nunavut currently only supports 1-byte bytes");
    nunavut::support::bitspan               heartbeat_span{reinterpret_cast<std::uint8_t*>(buffer),
                                             uavcan::node::Heartbeat_1_0::_traits_::ExtentBytes,
                                             0};
    nunavut::support::expected<std::size_t> serialize_result =
        uavcan::node::serialize(heartbeat_message, heartbeat_span);
    if (!serialize_result.has_value())
    {
        return 1;
    }
    std::cout << "Heartbeat message serialized successfully" << std::endl;
    if (!out->send(libcyphal::transport::Transfer{buffer, serialize_result.value()},
                   libcyphal::transport::TransferPriority::PriorityNominal,
                   std::chrono::milliseconds{0}))
    {
        return 1;
    }

    // +-----------------------------------------------------------------------+
    // | RUN CYCLE
    // +-----------------------------------------------------------------------+
    std::cout << "Heartbeat message sent successfully" << std::endl;
    if (!udp.runFor(std::chrono::milliseconds{100}))
    {
        return 1;
    }
    std::cout << "UDP Transport ran successfully" << std::endl;

    // +-----------------------------------------------------------------------+
    // | RECEIVE :: GET_INFO
    // +-----------------------------------------------------------------------+

    // this is where you left off.
    return 0;
}
