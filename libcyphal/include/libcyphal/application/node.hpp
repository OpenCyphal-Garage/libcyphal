/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Node Class

#ifndef LIBCYPHAL_APPLICATION_NODE_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_NODE_HPP_INCLUDED

#include "libcyphal/presentation/presentation.hpp"

namespace libcyphal
{
namespace application
{

class Node
{
public:
    /// @brief Constructor
    /// @param presentation Reference to the presentation layer passed in from the user
    Node(presentation::Presentation& presentation)
        : presentation_{presentation}
    {
    }

    virtual ~Node()         = default;
    Node(Node&)             = delete;
    Node(Node&&)            = delete;
    Node& operator=(Node&)  = delete;
    Node& operator=(Node&&) = delete;

    /// @brief Initializes the presentation layer
    /// @return Status of initializing the presentation layer
    Status initialize()
    {
        return presentation_.initialize();
    }

    /// @brief Calls the Presentation Layer's factory method to create a new Publisher object
    /// @return Publisher
    presentation::Publisher makePublisher()
    {
        return presentation_.makePublisher();
    }

    /// @brief Calls the Presentation Layer's factory method to create a new Subscriber object
    /// @return Subscriber
    presentation::Subscriber makeSubscriber()
    {
        return presentation_.makeSubscriber();
    }

    /// @brief Calls the Presentation Layer's factory method to create a new Client object
    /// @return Client
    presentation::Client makeClient()
    {
        return presentation_.makeClient();
    }

    /// @brief Calls the Presentation Layer's factory method to create a new Server object
    /// @return Server
    presentation::Server makeServer()
    {
        return presentation_.makeServer();
    }

    /// @brief Receives all incoming messages, requests, and responses for all registered
    ///        Port IDs and triggers the user-defined listener
    /// @note  This is currently a Node-level function since right now we have one input
    ///        session for all incoming transfers instead of one input session per Port ID.
    ///        This means we need to use the same Listener for all transfers.
    ///        Will be fixed by OVPG-3491.
    /// @param[in] listener The user-provided listener callback
    /// @return Status of receiving transfers
    Status receiveAllTransfers(transport::Listener& listener) const
    {
        return presentation_.receiveAllTransfers(listener);
    }

private:
    presentation::Presentation& presentation_;
};

}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_NODE_HPP_INCLUDED