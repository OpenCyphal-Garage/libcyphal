/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Transport Interface Wrapper for setting up a client to send requests and receive
/// responses over UDP

#ifndef POSIX_LIBCYPHAL_WRAPPERS_UDP_CLIENT_HPP_INCLUDED
#define POSIX_LIBCYPHAL_WRAPPERS_UDP_CLIENT_HPP_INCLUDED

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

/// @brief UDP Client is a wrapper around the tasks needed to send requests and receive responses
class Client final : public Base
{
public:
    /// @brief Constructor for UDP Client which is a wrapper around the tasks needed to send messages
    /// @param[in] ip_address Local IP Address
    /// @param[in] node_id The desired NodeID of the Transport
    Client(const transport::ip::v4::Address ip_address, const NodeID node_id, transport::Listener& listener) noexcept
        : Base(ip_address, node_id)
        , listener_{listener}
    {}

    virtual ~Client()           = default;
    Client(Client&)             = delete;
    Client(Client&&)            = delete;
    Client& operator=(Client&)  = delete;
    Client& operator=(Client&&) = delete;

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
        // is a client for multiple service IDs, it will receive all responses
        // on the same multicast address. For example, if the client Node ID
        // is 43 and it has registered for service IDs 409 and 410, it will 
        // receive responses for both of these services on the same multicast
        // address: 239.1.0.43
        result = interface_.setupServiceReceiver(node_id_);
        if (result.isFailure())
        {
            return result;
        }

        return Base::initialize();
    }

    /// @brief Registers a service ID for a client.
    /// @details A client sends requests and receives responses, so the
    ///          the service ID will be registered as a publication request
    ///          and as a subscription response
    /// @param[in] service_id Service ID to register
    Status registerServiceId(PortID service_id) const noexcept
    {
        Status result{};
        // Client sends requests
        result = udp_->registerPublication(service_id, transport::TransferKindRequest);
        if (result.isFailure())
        {
            return result;
        }

        // Client receives responses
        result = udp_->registerSubscription(service_id, transport::TransferKind::TransferKindResponse);
        return result;
    }

    /// @brief Wrapper to send multicast request
    /// @param[in] service_id Service ID of the transfer to send
    /// @param[in] remote_node_id The Node ID to whom the request will be sent
    /// @param[in] buffer The transfer payload
    /// @param[in] buffer_size The size of the transfer
    Status sendRequest(PortID service_id, NodeID remote_node_id, const std::uint8_t* buffer, const std::size_t buffer_size)
    {
        Span<const std::uint8_t> span_transfer{buffer, buffer_size};
        return udp_->sendRequest(service_id, remote_node_id, span_transfer);
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

#endif  // POSIX_LIBCYPHAL_WRAPPERS_UDP_CLIENT_HPP_INCLUDED
