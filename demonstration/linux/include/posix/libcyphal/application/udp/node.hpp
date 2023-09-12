/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Implementation of the UDP Node. This is an example wrapper class around the generic
/// Node class to make integration easier for users. Note the order of creation is as
/// follows: First, create the transport layer. Then pass the transport to the Presentation
/// layer. And finally, pass the presentation layer to the Node application layer.

#ifndef LIBCYPHAL_POSIX_APPLICATION_UDP_NODE_HPP_INCLUDED
#define LIBCYPHAL_POSIX_APPLICATION_UDP_NODE_HPP_INCLUDED

#include "libcyphal/application/node.hpp"
#include "libcyphal/transport/ip/v4/address.hpp"
#include "libcyphal/transport/udp/cyphal_udp_transport.hpp"
#include "libcyphal/transport/udp/udp_interface.hpp"

#include "posix/libcyphal/transport/udp/session/input_session.hpp"
#include "posix/libcyphal/transport/udp/session/output_session.hpp"
#include "posix/libcyphal/types/o1_heap.hpp"
#include "posix/libcyphal/types/posix_time.hpp"
#include "posix/libcyphal/types/udpard_heap.hpp"

namespace libcyphal
{
namespace posix
{
namespace application
{
namespace udp
{

class UDPNode
{
public:
    /// @brief Constructor
    /// @param ip_address The local IP Address of the Node
    /// @param node_id The local Node ID
    /// @param heap The user-provided memory for udpard
    UDPNode(const transport::ip::v4::Address ip_address, const NodeID node_id, Heap& heap) noexcept
        : input_session_{transport::udp::session::PosixInputSession(node_id, ip_address)}
        , output_session_{transport::udp::session::PosixOutputSession(node_id, ip_address)}
        , primary_bus_{transport::udp::UDPInterface(input_session_, output_session_)}
        , timer_{time::PosixTimer()}
        , udp_transport_{new transport::udp::CyphalUDPTransport(primary_bus_,
                                                                nullptr,
                                                                node_id,
                                                                timer_,
                                                                heap,
                                                                &udpardMemAllocate,
                                                                &udpardMemFree)}
        , presentation_{libcyphal::presentation::Presentation(*udp_transport_)}
        , udp_node_{new libcyphal::application::Node(presentation_)}
    {
    }

    /// @brief Cleanup pointers
    ~UDPNode() noexcept
    {
        delete udp_transport_;
        delete udp_node_;
    }

    UDPNode(UDPNode&)             = delete;
    UDPNode(UDPNode&&)            = delete;
    UDPNode& operator=(UDPNode&)  = delete;
    UDPNode& operator=(UDPNode&&) = delete;

    /// @brief Initializes the application layer
    /// @return Status of initializing the application layer
    Status initialize()
    {
        return udp_node_->initialize();
    }

    /// @brief Calls the Node's factory method to create a new Publisher object
    /// @return Publisher
    presentation::Publisher makePublisher()
    {
        return udp_node_->makePublisher();
    }

    /// @brief Calls the Node's factory method to create a new Subscriber object
    /// @return Subscriber
    presentation::Subscriber makeSubscriber()
    {
        return udp_node_->makeSubscriber();
    }

    /// @brief Calls the Node's factory method to create a new Client object
    /// @return Client
    presentation::Client makeClient()
    {
        return udp_node_->makeClient();
    }

    /// @brief Calls the Node's factory method to create a new Server object
    /// @return Server
    presentation::Server makeServer()
    {
        return udp_node_->makeServer();
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
        return udp_node_->receiveAllTransfers(listener);
    }

private:
    transport::udp::session::PosixInputSession  input_session_;
    transport::udp::session::PosixOutputSession output_session_;
    transport::udp::UDPInterface                primary_bus_;
    const time::PosixTimer                      timer_;
    transport::udp::CyphalUDPTransport*         udp_transport_;
    libcyphal::presentation::Presentation       presentation_;
    libcyphal::application::Node*               udp_node_;
};

}  // namespace udp
}  // namespace application
}  // namespace posix
}  // namespace libcyphal

#endif  // LIBCYPHAL_POSIX_APPLICATION_UDP_NODE_HPP_INCLUDED