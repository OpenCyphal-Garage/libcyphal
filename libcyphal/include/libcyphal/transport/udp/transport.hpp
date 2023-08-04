/// @file
/// Cyphal Transport Interface implementation used to communicate over a UDP bus
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_TRANSPORT_UDP_UDP_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_UDP_HPP_INCLUDED

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>

#include "cetl/pf17/memory_resource.hpp"

#include "libcyphal/introspection.hpp"
#include "libcyphal/transport/transport.hpp"
#include "libcyphal/transport/udp/session/input.hpp"
#include "libcyphal/transport/udp/session/output.hpp"
#include "libcyphal/transport/udp/ard.h"

#include "libcyphal/network/ip/udp.hpp"
#include "libcyphal/network/context.hpp"
#include "libcyphal/network/ip/socket.hpp"
#include "libcyphal/network/ip/address.hpp"

namespace libcyphal
{
namespace transport
{
namespace udp
{

struct TransportMemoryResources
{
    cetl::pf17::pmr::memory_resource* input_session_memory;
    cetl::pf17::pmr::memory_resource* output_session_memory;
    cetl::pf17::pmr::memory_resource* tx_queue_memory;
    // TODO [Scott]: update 'ards so this can be two separate memory resources.
    //      [Pavel]: This is done in the main2 branch, see struct UdpardRxMemoryResources.
    cetl::pf17::pmr::memory_resource* rx_payload_buffer_and_session_memory;
};

/// @brief The UDP implementation of the transport interface to communicate over UDP
class Transport final : public ITransport, public IRunnable
{
public:
    /// @brief  Per [Table 4.5] of the Cyphal specification.
    static constexpr std::size_t MaxNodeIdValue = 65534;

    ///
    /// @param local_ip_address: Specifies which local network interface to use for this transport.
    ///
    ///     Using ``INADDR_ANY`` here (i.e., ``0.0.0.0`` for IPv4) is not expected to work reliably or be portable
    ///     because this configuration is, generally, incompatible with multicast sockets (even in the anonymous mode).
    ///     In order to set up even a listening multicast socket, it is necessary to specify the correct local
    ///     address such that the underlying IP stack is aware of which interface to receive multicast packets from.
    ///
    ///     When the anonymous mode is enabled, it is quite possible to snoop on the network even if there is
    ///     another node running locally on the same interface
    ///     (because sockets are initialized with ``SO_REUSEADDR`` and ``SO_REUSEPORT``, when available).
    ///
    /// @param local_node_id: As explained previously, the node-ID is part of the UDP Frame.
    ///
    ///     - If the value is None, an anonymous instance will be constructed.
    ///       Emitted UDP frames will then report its :attr:`source_node_id` as None.
    ///
    ///     - If the value is a non-negative integer, then we can setup both input and output sessions.
    ///
    /// @param mtu_bytes: The application-level MTU for outgoing packets.
    ///     In other words, this is the maximum number of serialized bytes per Cyphal/UDP frame.
    ///     Transfers where the number of payload bytes does not exceed this value will be single-frame transfers,
    ///     otherwise, multi-frame transfers will be used.
    ///     This setting affects only outgoing frames;
    ///     the MTU of incoming frames is fixed at a sufficiently large value to accept any meaningful UDP frame.
    ///
    ///     The default value is the smallest valid value for reasons of compatibility.
    ///
    Transport(network::IContext&         network_context,
              janky::optional<NodeID>    local_node_id,
              TransportMemoryResources&& memory_resources,
              std::uint32_t              mtu_bytes = DefaultMTU)
        : network_context_{network_context}
        , local_node_id_(local_node_id)
        , mtu_bytes_{mtu_bytes}
        , input_registry_allocator_{memory_resources.input_session_memory}
        , output_registry_allocator_{memory_resources.output_session_memory}
        , closed_{false}
        , tx_context_{local_node_id.value_or(AnonymousNodeID), memory_resources.tx_queue_memory}
        , rx_context_{local_node_id.value_or(AnonymousNodeID), memory_resources.rx_payload_buffer_and_session_memory}
        , interfaces_{}
        , input_registry_{input_registry_allocator_}
        , output_registry_{output_registry_allocator_}
        , protocol_parameters_{janky::unexpected<ResultCode>{ResultCode::UninitializedError}}
    {
        CETL_DEBUG_ASSERT(memory_resources.input_session_memory, "input_session_memory cannot be null");
        CETL_DEBUG_ASSERT(memory_resources.output_session_memory, "output_session_memory cannot be null");
        (void) std::move(memory_resources);
    }

    ~Transport() noexcept final            = default;
    Transport(const Transport&)            = delete;
    Transport(Transport&&)                 = delete;
    Transport& operator=(const Transport&) = delete;
    Transport& operator=(Transport&&)      = delete;

    // +-----------------------------------------------------------------------+
    // | IRunnable
    // +-----------------------------------------------------------------------+
    Status runFor(std::chrono::microseconds max_run_duration) noexcept override
    {
        (void) max_run_duration;
        // TODO: read ards and do stuff.
        return ResultCode::Success;
    }

    Status cancel() noexcept override
    {
        return ResultCode::NotImplementedError;
    }
    // +-----------------------------------------------------------------------+
    // | ITransport
    // +-----------------------------------------------------------------------+
    Status initialize() override
    {
        if (local_node_id_ && *local_node_id_ >= 0xFFFF)
        {
            return ResultCode::InvalidArgumentError;
        }

        auto& interface_enumerator = network_context_.enumerateIPInterfaces();
        CETL_DEBUG_ASSERT(interface_enumerator.count() > 0, "interface_count must be greater than 0");
        CETL_DEBUG_ASSERT(interface_enumerator.count() <= interfaces_.max_size(),
                          "Too many interfaces requested. Max is set by maxRedundantInterfaces in libcyphal.hpp");

        for (auto& interface_address : network_context_.enumerateIPInterfaces())
        {
            interfaces_.emplace_back(interface_address, mtu_bytes_, tx_context_);
        }

        return ResultCode::Success;
    }

    ProtocolParameters getProtocolParameters() const noexcept override
    {
        return ProtocolParameters{std::numeric_limits<TransferID>::max(), MaxNodeIdValue, mtu_bytes_};
    }

    janky::optional<NodeID> getLocalNodeId() const override
    {
        return local_node_id_;
    }

    void close() override
    {
        // TODO: close all the things!
        // presumably this will involve closing each and every session.
    }

    Status getInputSession(InputSessionSpecifier specifier,
                           PayloadMetadata       payload_metadata,
                           IInputSession*&       out) override
    {
        out = nullptr;
        out = nullptr;
        if (closed_)
        {
            return ResultCode::ResourceClosedError;
        }
        auto it = input_registry_.find(specifier);
        if (it != input_registry_.end())
        {
            out = &(it->second);
            return ResultCode::Success;
        }

        // check if anonymous, in that case no service transfers are allowed
        if (!local_node_id_ && specifier.getDataSpecifier().isService())
        {
            // Anonymous UDP Transport cannot create service output session
            return ResultCode::InvalidArgumentError;
        }

        session::RedundantNetworkRxInterfaceContainer rx_interfaces{};
        auto                                          read_poll_perhaps = network_context_.makeReadPoll();

        if (!read_poll_perhaps.has_value())
        {
            return read_poll_perhaps.error();
        }

        auto&                      read_poll_ptr     = *read_poll_perhaps;
        const network::ip::Address multicast_address = toMulticastAddress(specifier);
        // For now we just enumerate the tx_contexts but we only use their IP
        // addresses.
        for (UdpardTxContext& network_interface : interfaces_)
        {
            auto&& ip_socket_perhaps =
                network_context_.makeBoundUdpMulticastInputSocket(network_interface.interface_address,
                                                                  multicast_address,
                                                                  network::ip::udp::CyphalPort);
            if (!ip_socket_perhaps.has_value())
            {
                return Status{ip_socket_perhaps.error(), 0x50};
            }
            rx_interfaces.v().emplace_back(std::move(ip_socket_perhaps.value()));
        }

        input_registry_.emplace(specifier,
                                session::UDPInputSession{specifier,
                                                         payload_metadata,
                                                         rx_context_,
                                                         std::move(rx_interfaces),
                                                         std::move(read_poll_ptr)});
        session::UDPInputSession& session = input_registry_.at(specifier);
        const Status              r       = session.initialize();
        if (!r)
        {
            input_registry_.erase(specifier);
            return r;
        }
        out = &session;
        return ResultCode::Success;
    }

    Status getOutputSession(OutputSessionSpecifier specifier,
                            PayloadMetadata        payload_metadata,
                            IOutputSession*&       out) override
    {
        out = nullptr;
        if (closed_)
        {
            return ResultCode::ResourceClosedError;
        }
        auto it = output_registry_.find(specifier);
        if (it != output_registry_.end())
        {
            out = &(it->second);
            return ResultCode::Success;
        }

        // check if anonymous, in that case no service transfers are allowed
        if (!local_node_id_ && specifier.getDataSpecifier().isService())
        {
            // Anonymous UDP Transport cannot create service output session
            return ResultCode::InvalidArgumentError;
        }

        session::RedundantNetworkTxInterfaceContainer tx_interfaces{};

        const network::ip::Address multicast_address = toMulticastAddress(specifier);
        for (UdpardTxContext& tx_queue : interfaces_)
        {
            auto&& ip_socket_perhaps = network_context_.makeBoundUdpMulticastOutputSocket(tx_queue.interface_address);
            if (!ip_socket_perhaps.has_value())
            {
                return Status{ip_socket_perhaps.error(), 0x55};
            }
            network::SocketPointer<network::ip::Socket>&& ip_socket = std::move(ip_socket_perhaps.value());
            const Status r = ip_socket->connect(multicast_address, network::ip::udp::CyphalPort);
            if (r != ResultCode::Success)
            {
                return Status{r.result, 0x56};
            }
            tx_interfaces.v().emplace_back(&tx_queue, std::move(ip_socket));
        }

        output_registry_.emplace(specifier,
                                 session::UDPOutputSession{specifier, payload_metadata, std::move(tx_interfaces)});
        session::UDPOutputSession& session = output_registry_.at(specifier);
        const Status               r       = session.initialize();
        if (!r)
        {
            return r;
        }
        out = &session;
        return ResultCode::Success;
    }

private:
    static network::ip::Address toMulticastAddress(const SessionSpecifier& specifier) noexcept
    {
        const DataSpecifier& data_specifier = specifier.getDataSpecifier();
        const auto           remote_node_id = specifier.getRemoteNodeId();
        std::uint32_t        remote_ip      = 0;
        DataSpecifier::Role  role           = data_specifier.getRole();

        if (role == DataSpecifier::Role::Message)
        {
            CETL_DEBUG_ASSERT(!remote_node_id.has_value(), "Message data specifier cannot have a remote node ID");
            remote_ip = network::ip::udp::messageDataSpecifierToIPV4CIV0MulticastGroup(data_specifier.getId());
        }
        else
        {
            CETL_DEBUG_ASSERT(remote_node_id.has_value(), "Service data specifier must have a remote node ID");
            remote_ip = network::ip::udp::serviceNodeIDToIPV4CIV0MulticastGroup(*remote_node_id);
        }
        return network::ip::Address{remote_ip};
    }

    // +-----------------------------------------------------------------------+
    // | PRIVATE
    // +-----------------------------------------------------------------------+
    network::IContext&      network_context_;
    janky::optional<NodeID> local_node_id_;
    std::uint32_t           mtu_bytes_;
    cetl::pf17::pmr::polymorphic_allocator<std::pair<const InputSessionSpecifier, session::UDPInputSession>>
        input_registry_allocator_;
    cetl::pf17::pmr::polymorphic_allocator<std::pair<const OutputSessionSpecifier, session::UDPOutputSession>>
                                                                                          output_registry_allocator_;
    bool                                                                                  closed_;
    UdpardContext                                                                         tx_context_;
    UdpardContext                                                                         rx_context_;
    libcyphal::janky::UnsynchronizedStaticVector<UdpardTxContext, maxRedundantInterfaces> interfaces_;
    std::unordered_map<InputSessionSpecifier,
                       session::UDPInputSession,
                       std::hash<InputSessionSpecifier>,
                       std::equal_to<InputSessionSpecifier>,
                       decltype(input_registry_allocator_)>
        input_registry_;
    std::unordered_map<OutputSessionSpecifier,
                       session::UDPOutputSession,
                       std::hash<OutputSessionSpecifier>,
                       std::equal_to<OutputSessionSpecifier>,
                       decltype(output_registry_allocator_)>
                                        output_registry_;
    janky::expected<ProtocolParameters> protocol_parameters_;
};

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_UDP_HPP_INCLUDED
