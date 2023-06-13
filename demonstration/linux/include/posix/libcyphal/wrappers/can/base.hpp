/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Transport Interface implementation used to communicate over a UDP bus

#ifndef POSIX_LIBCYPHAL_WRAPPERS_CAN_BASE_HPP_INCLUDED
#define POSIX_LIBCYPHAL_WRAPPERS_CAN_BASE_HPP_INCLUDED

#include <cstddef>
#include <cstdint>
#include <memory>
#include <libcyphal/types/status.hpp>
#include <libcyphal/transport/can/cyphal_can_transport.hpp>
#include <libcyphal/transport/can/transport.hpp>
#include <libcyphal/transport/message.hpp>
#include <libcyphal/transport/id_types.hpp>
#include "posix/libcyphal/transport/can/connection.hpp"
#include "posix/libcyphal/transport/can/session/input_session.hpp"
#include "posix/libcyphal/transport/can/session/output_session.hpp"
#include "posix/libcyphal/types/posix_time.hpp"
#include "posix/libcyphal/types/canard_heap.hpp"

namespace libcyphal
{
namespace wrappers
{
namespace can
{

/// Warning: The Libcyphal API is undergoing a redesign and this class will be going
/// away soon: https://jira.adninfra.net/browse/OVPG-3288

/// Base class for Transport Nodes that wraps common setups for sending/receiving messages
class Base
{
public:
    NodeID                 node_id_;
    const time::PosixTimer timer_;

    /// @brief Base class constructor for Transport Node wrapper for sending/receiving messages
    /// @param[in] can_interface The CAN Interface name to use
    /// @param[in] node_id The desired NodeID of the Transport
    Base(const char* can_interface, const NodeID node_id, cetl::pf17::pmr::memory_resource* resource) noexcept
        : node_id_{node_id}
        , timer_{time::PosixTimer()}
        , interface_{transport::can::CANTransport(input_session_, output_session_)}
        , can_{new transport::can::CyphalCANTransport(static_cast<TransportID>(node_id),
                                                      interface_,
                                                      nullptr,
                                                      timer_,
                                                      resource,
                                                      &canardMemAllocate,
                                                      &canardMemFree)}
        , output_session_{transport::can::session::PosixOutputSession(node_id, can_interface)}
        , input_session_{transport::can::session::PosixInputSession(node_id, can_interface)}
    {}

    /// @brief Base class constructor for Transport Node wrapper for sending/receiving messages
    /// @param[in] node_id The desired NodeID of the Transport
    Base(const NodeID node_id, cetl::pf17::pmr::memory_resource* resource) noexcept
        : Base("", node_id, resource)
    {}

    /// @brief Common initialization steps for setting up common Node initialization steps
    virtual Status initialize()
    {
        Status result{};
        result = can_->initialize();
        can_->setNodeID(node_id_);

        return result;
    }

protected:
    transport::can::CANTransport        interface_;
    transport::can::CyphalCANTransport* can_;

    /// @brief Cleanup pointers and connections
    ~Base() noexcept
    {
        delete can_;
    }

private:
    transport::can::session::PosixOutputSession output_session_;
    transport::can::session::PosixInputSession  input_session_;
};

}  // namespace can
}  // namespace wrappers
}  // namespace libcyphal

#endif  // POSIX_LIBCYPHAL_WRAPPERS_CAN_BASE_HPP_INCLUDED
