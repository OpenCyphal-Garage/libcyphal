/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Transport Interface implementation used to communicate over a UDP bus

#ifndef POSIX_LIBCYPHAL_WRAPPERS_UDP_BASE_HPP_INCLUDED
#define POSIX_LIBCYPHAL_WRAPPERS_UDP_BASE_HPP_INCLUDED

#include <memory>
#include <cstddef>
#include <cstdint>
#include <libcyphal/types/status.hpp>
#include <libcyphal/transport/id_types.hpp>
#include <libcyphal/transport/ip/v4/address.hpp>
#include <libcyphal/transport/message.hpp>
#include <libcyphal/transport/udp/cyphal_udp_transport.hpp>
#include <libcyphal/transport/udp/transport.hpp>
#include "posix/libcyphal/transport/ip/v4/connection.hpp"
#include "posix/libcyphal/transport/udp/session/input_session.hpp"
#include "posix/libcyphal/transport/udp/session/output_session.hpp"
#include "posix/libcyphal/types/posix_time.hpp"
#include "posix/libcyphal/types/udpard_heap.hpp"

#include "cetl/pf17/memory_resource.hpp"

namespace libcyphal
{
namespace wrappers
{
namespace udp
{

/// Warning: The Libcyphal API is undergoing a redesign and this class will be going
/// away soon: https://jira.adninfra.net/browse/OVPG-3288

/// Base class for Transport Nodes that wraps common setups for sending/receiving messages
class Base
{
public:
    transport::ip::v4::Address        local_ip_;
    NodeID                            node_id_;
    cetl::pf17::pmr::memory_resource* resource_;
    const time::PosixTimer            timer_;

    /// @brief Base class constructor for Transport Node wrapper for sending/receiving messages
    /// @param[in] ip_address The local ip address of the node
    /// @param[in] node_id The desired NodeID of the Transport
    Base(const transport::ip::v4::Address  ip_address,
         const NodeID                      node_id,
         cetl::pf17::pmr::memory_resource* resource) noexcept
        : local_ip_{ip_address}
        , node_id_{node_id}
        , resource_{resource}
        , timer_{time::PosixTimer()}
        , interface_{transport::udp::UDPTransport(input_session_, output_session_)}
        , udp_{new transport::udp::CyphalUDPTransport(interface_,
                                                      nullptr,
                                                      node_id_,
                                                      timer_,
                                                      resource_,
                                                      &udpardMemAllocate,
                                                      &udpardMemFree)}
        , output_session_{transport::udp::session::PosixOutputSession(node_id, ip_address)}
        , input_session_{transport::udp::session::PosixInputSession(node_id, ip_address)}
    {}

    /// @brief Common initialization steps for setting up common Node initialization steps
    virtual Status initialize()
    {
        return udp_->initialize();
    }

protected:
    transport::udp::UDPTransport        interface_;
    transport::udp::CyphalUDPTransport* udp_;

    /// @brief Cleanup pointers and connections
    ~Base() noexcept
    {
        delete udp_;
    }

private:
    transport::udp::session::PosixOutputSession output_session_;
    transport::udp::session::PosixInputSession  input_session_;
};

}  // namespace udp
}  // namespace wrappers
}  // namespace libcyphal

#endif  // POSIX_LIBCYPHAL_WRAPPERS_UDP_BASE_HPP_INCLUDED
