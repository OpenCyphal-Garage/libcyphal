/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Interface for Servers

#ifndef LIBCYPHAL_PRESENTATION_SERVER_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_SERVER_HPP_INCLUDED

#include "libcyphal/transport.hpp"
#include "libcyphal/transport/id_types.hpp"
#include "libcyphal/types/span.hpp"
#include "libcyphal/types/status.hpp"

namespace libcyphal
{
namespace presentation
{

class IServer
{
public:
    /// @brief Registers a service ID for a server.
    /// @details A server receives requests and sends responses, so the
    ///          the service ID will be registered as an incoming request
    ///          and as an outgoing response
    /// @param[in] service_id Service ID to register
    /// @return Status of registering the service ID
    virtual Status registerServiceID(PortID service_id) = 0;

    /// @brief Sends a serialized response to specified node
    /// @param[in] service_id Service ID of the response to send
    /// @param[in] remote_node_id The Node ID to whom the response will be sent
    /// @param[in] payload The response payload
    /// @param[in] size The size of the payload
    /// @return Status of sending response
    virtual Status respond(PortID              service_id,
                           NodeID              remote_node_id,
                           const std::uint8_t* payload,
                           const std::size_t   size) = 0;

    IServer()                     = default;
    IServer(IServer&)             = delete;
    IServer& operator=(IServer&)  = delete;
    IServer& operator=(IServer&&) = delete;

protected:
    virtual ~IServer() = default;
};

/// @brief Implementation for IServer
class Server : public IServer
{
public:
    /// @brief Constructor
    /// @param transport The concrete underlying transport provided by the user
    Server(libcyphal::Transport& transport)
        : transport_{&transport}
    {
    }

    /// @brief Move Constructor
    /// @param[in] other Server to move from
    Server(Server&& other) noexcept
        : transport_{other.transport_}
    {
        other.transport_ = nullptr;
    }

    /// @brief Registers a service ID for a server.
    /// @details A server receives requests and sends responses, so the
    ///          the service ID will be registered as an incoming request
    ///          and as an outgoing response
    /// @param[in] service_id Service ID to register
    /// @return Status of registering the service ID
    Status registerServiceID(PortID service_id) override
    {
        if (transport_ == nullptr)
        {
            return ResultCode::NotInitialized;
        }

        Status result{};
        // Server sends responses
        result = transport_->registerPublication(service_id, transport::TransferKindResponse);
        if (result.isFailure())
        {
            return result;
        }

        // Server receives requests
        result = transport_->registerSubscription(service_id, transport::TransferKind::TransferKindRequest);
        return result;
    }

    /// @brief Sends a serialized response to specified node
    /// @param[in] service_id Service ID of the response to send
    /// @param[in] remote_node_id The Node ID to whom the response will be sent
    /// @param[in] payload The response payload
    /// @param[in] size The size of the payload
    /// @return Status of sending response
    Status respond(PortID              service_id,
                   NodeID              remote_node_id,
                   const std::uint8_t* buffer,
                   const std::size_t   buffer_size) override
    {
        if (transport_ == nullptr)
        {
            return ResultCode::NotInitialized;
        }

        Span<const std::uint8_t> span_transfer{buffer, buffer_size};
        return transport_->sendResponse(service_id, remote_node_id, span_transfer);
    }

private:
    libcyphal::Transport* transport_{nullptr};
};

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_SERVER_HPP_INCLUDED