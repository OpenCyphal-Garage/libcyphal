/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Presentation Class that accepts a Transport object and provides the factory methods
/// for creating new Publishers, Subscribers, Clients, and Servers

#ifndef LIBCYPHAL_PRESENTATION_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_HPP_INCLUDED

#include "libcyphal/presentation/publisher.hpp"
#include "libcyphal/presentation/subscriber.hpp"

#include "libcyphal/presentation/client.hpp"
#include "libcyphal/presentation/server.hpp"

#include "libcyphal/transport/listener.hpp"

namespace libcyphal
{
namespace presentation
{

class Presentation
{
public:
    /// @brief Constructor
    /// @param[in] transport The concrete underlying transport, e.g. UDP or CAN, provided by the user
    Presentation(Transport& transport)
        : transport_{&transport}
    {
    }

    /// @brief Move Constructor
    /// @param[in] other Publisher object to move from
    Presentation(Presentation&& other) noexcept
        : transport_{other.transport_}
    {
        other.transport_ = nullptr;
    }

    virtual ~Presentation()                 = default;
    Presentation(Presentation&)             = delete;
    Presentation& operator=(Presentation&)  = delete;
    Presentation& operator=(Presentation&&) = delete;

    /// @brief Initializes the transport layer
    /// @return Status of initializing the transport layer
    Status initialize()
    {
        if (transport_ == nullptr)
        {
            return ResultCode::NotInitialized;
        }

        return transport_->initialize();
    }

    /// @brief Creates a new Publisher object
    /// @note Right now, one publisher can broadcast multiple subject IDs but in the future
    ///       one publisher will be associated with only one Subject ID (OVPG-3491)
    /// @return Publisher
    Publisher makePublisher()
    {
        return Publisher{*transport_};
    }

    /// @brief Creates a new Subscriber object
    /// @note Right now, one subscriber can receive multiple subject IDs but in the future
    ///       one subscriber will be associated with only one Subject ID (OVPG-3491)
    /// @return Subscriber
    Subscriber makeSubscriber()
    {
        return Subscriber{*transport_};
    }

    /// @brief Creates a new Client object
    /// @note Right now, one client can register multiple service IDs but in the future
    ///       one client will be associated with only one service ID (OVPG-3491)
    /// @return Client
    Client makeClient()
    {
        return Client{*transport_};
    }

    /// @brief Creates a new Server object
    /// @note Right now, one server can register multiple service IDs but in the future
    ///       one server will be associated with only one service ID (OVPG-3491)
    /// @return Server
    Server makeServer()
    {
        return Server{*transport_};
    }

    /// @brief Receives all incoming message and service transfers for all registered
    ///        Port IDs and triggers the user-defined listener
    /// @note  This is currently a Node-level function since right now we have one input
    ///        session for all incoming transfers instead of one input session per Port ID.
    ///        This means we need to use the same Listener for all transfers.
    ///        Will be fixed by OVPG-3491.
    /// @param[in] listener The user-provided listener callback
    /// @return Status of receiving transfers
    Status receiveAllTransfers(transport::Listener& listener) const
    {
        if (transport_ == nullptr)
        {
            return ResultCode::NotInitialized;
        }

        return transport_->processIncomingTransfers(listener);
    }

private:
    Transport* transport_{nullptr};
};

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_HPP_INCLUDED