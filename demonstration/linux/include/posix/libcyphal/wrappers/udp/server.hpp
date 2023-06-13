/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Transport Interface Wrapper for setting up a server to receive requests and
/// send responses over UDP

#ifndef POSIX_LIBCYPHAL_WRAPPERS_UDP_SERVER_HPP_INCLUDED
#define POSIX_LIBCYPHAL_WRAPPERS_UDP_SERVER_HPP_INCLUDED

#include <cstddef>
#include <cstdint>
#include <libcyphal/transport/id_types.hpp>
#include <libcyphal/types/status.hpp>
#include "posix/libcyphal/wrappers/udp/base.hpp"

namespace libcyphal
{
namespace wrappers
{
namespace udp
{

/// Warning: The Libcyphal API is undergoing a redesign and these wrapper classes will be going
/// away soon: https://jira.adninfra.net/browse/OVPG-3288

/// @brief UDP Server is a wrapper around the tasks needed to send requests and receive responses
class Server final : public Base
{
public:
    /// @brief Constructor for UDP Server which is a wrapper around the tasks needed to send messages
    /// @param[in] ip_address Local IP Address
    /// @param[in] node_id The desired NodeID of the Transport
    Server(const transport::ip::v4::Address  ip_address,
           const NodeID                      node_id,
           transport::Listener&              listener,
           cetl::pf17::pmr::memory_resource* resource) noexcept
        : Base(ip_address, node_id, resource)
        , listener_{listener}
    {
    }

    virtual ~Server()           = default;
    Server(Server&)             = delete;
    Server(Server&&)            = delete;
    Server& operator=(Server&)  = delete;
    Server& operator=(Server&&) = delete;

    /// @brief Initializes everything needed to send and receive frames
    Status initialize() override
    {
        Status result{};
        result = interface_.initializeOutput();
        if (result.isFailure())
        {
            return result;
        }

        result = interface_.initializeInput();
        if (result.isFailure())
        {
            return result;
        }

        // Only need to setup the receiver once because even if a Node is
        // is a server for multiple service IDs, it will receive all requests
        // on the same multicast address. For example, if the server Node ID
        // is 44 and it has registered for service IDs 409 and 410, it will
        // receive requests for both of these services on the same multicast
        // address: 239.1.0.44
        result = interface_.setupServiceReceiver(node_id_);
        if (result.isFailure())
        {
            return result;
        }

        return Base::initialize();
    }

    /// @brief Registers a service ID for a server.
    /// @details A server receives requests and sends responses, so the
    ///          the service ID will be registered as a publication response
    ///          and as a subscription request
    /// @param[in] service_id Service ID to register
    Status registerServiceId(PortID service_id) const noexcept
    {
        Status result{};
        // Server sends responses
        result = udp_->registerPublication(service_id, transport::TransferKindResponse);
        if (result.isFailure())
        {
            return result;
        }

        // Server receives requests
        result = udp_->registerSubscription(service_id, transport::TransferKind::TransferKindRequest);
        return result;
    }

    /// @brief Wrapper to send multicast response
    /// @param[in] service_id Service ID of the transfer to send
    /// @param[in] remote_node_id The Node ID to whom the response will be sent
    /// @param[in] buffer The transfer payload
    /// @param[in] buffer_size The size of the transfer
    Status sendResponse(PortID              service_id,
                        NodeID              remote_node_id,
                        const std::uint8_t* buffer,
                        const std::size_t   buffer_size)
    {
        cetl::pf20::span<const std::uint8_t> span_transfer{buffer, buffer_size};
        return udp_->sendResponse(service_id, remote_node_id, span_transfer);
    }

    /// @brief Receives any incoming frames and triggers the listener as needed
    Status receive() const
    {
        return udp_->processIncomingTransfers(listener_);
    }

private:
    transport::Listener& listener_;
};

}  // namespace udp
}  // namespace wrappers
}  // namespace libcyphal

#endif  // POSIX_LIBCYPHAL_WRAPPERS_UDP_SERVER_HPP_INCLUDED
