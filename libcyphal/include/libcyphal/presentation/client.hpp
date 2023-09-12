/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Interface for Clients

#ifndef LIBCYPHAL_PRESENTATION_CLIENT_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_CLIENT_HPP_INCLUDED

#include "libcyphal/transport/id_types.hpp"

#include "libcyphal/types/span.hpp"
#include "libcyphal/types/status.hpp"
#include "libcyphal/transport.hpp"

namespace libcyphal
{
namespace presentation
{

class IClient
{
public:
    /// @brief Registers a service ID for a client.
    /// @details A client sends requests and receives responses, so the
    ///          the service ID will be registered as an outgoing request
    ///          and as an incoming response
    /// @param[in] service_id Service ID to register
    /// @return Status of registering the service ID
    virtual Status registerServiceID(PortID service_id) = 0;

    /// @brief Sends a serialized request to specified node
    /// @param[in] service_id Service ID of the request to send
    /// @param[in] remote_node_id The Node ID to whom the request will be sent
    /// @param[in] payload The request payload
    /// @param[in] size The size of the payload
    /// @return Status of sending request
    virtual Status request(PortID              service_id,
                           NodeID              remote_node_id,
                           const std::uint8_t* payload,
                           const std::size_t   size) = 0;

    IClient()                     = default;
    IClient(IClient&)             = delete;
    IClient& operator=(IClient&)  = delete;
    IClient& operator=(IClient&&) = delete;

protected:
    virtual ~IClient() = default;
};

/// @brief Implementation for the Client
class Client : public IClient
{
public:
    /// @brief Constructor
    /// @param transport The concrete underlying transport provided by the user
    Client(libcyphal::Transport& transport)
        : transport_{&transport}
    {
    }

    /// @brief Move Constructor
    /// @param[in] other Client to move from
    Client(Client&& other) noexcept
        : transport_{other.transport_}
    {
        other.transport_ = nullptr;
    }


    /// @brief Registers a service ID for a client.
    /// @details A client sends requests and receives responses, so the
    ///          the service ID will be registered as an outgoing request
    ///          and as an incoming response
    /// @param[in] service_id Service ID to register
    /// @return Status of registering the service ID
    Status registerServiceID(PortID service_id) override
    {
        if (transport_ == nullptr)
        {
            return ResultCode::NotInitialized;
        }

        Status result{};
        // Client sends requests
        result = transport_->registerPublication(service_id, transport::TransferKindRequest);
        if (result.isFailure())
        {
            return result;
        }

        // Client receives responses
        result = transport_->registerSubscription(service_id, transport::TransferKind::TransferKindResponse);
        return result;
    }

    /// @brief Sends a serialized request to specified node
    /// @param[in] service_id Service ID of the request to send
    /// @param[in] remote_node_id The Node ID to whom the request will be sent
    /// @param[in] payload The request payload
    /// @param[in] size The size of the payload
    /// @return Status of sending request
    Status request(PortID              service_id,
                   NodeID              remote_node_id,
                   const std::uint8_t* buffer,
                   const std::size_t   buffer_size) override
    {
        if (transport_ == nullptr)
        {
            return ResultCode::NotInitialized;
        }

        Span<const std::uint8_t> span_transfer{buffer, buffer_size};
        return transport_->sendRequest(service_id, remote_node_id, span_transfer);
    }

private:
    libcyphal::Transport* transport_{nullptr};
};

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_CLIENT_HPP_INCLUDED
