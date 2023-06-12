/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Transport Interface Wrapper for sending message over an Ethernet bus in POSIX

#ifndef POSIX_LIBCYPHAL_WRAPPERS_UDP_BROADCASTER_HPP_INCLUDED
#define POSIX_LIBCYPHAL_WRAPPERS_UDP_BROADCASTER_HPP_INCLUDED

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

/// @brief UDPSubscriber is a wrapper around the tasks needed to send messages
class Broadcaster final : public Base
{
public:
    /// @brief Constructor for UDPBroadcaster which is a wrapper around the tasks needed to send messages
    /// @param[in] ip_address Local IP Address
    /// @param[in] node_id The desired NodeID of the Transport
    Broadcaster(const transport::ip::v4::Address  ip_address,
                const NodeID                      node_id,
                cetl::pf17::pmr::memory_resource* resource) noexcept
        : Base(ip_address, node_id, resource)
    {
    }

    /// Destructor
    ~Broadcaster() = default;

    /// @brief Initializes everything needed to send frames
    Status initialize() override
    {
        Status result{};
        result = interface_.initializeOutput();
        if (result.isFailure())
        {
            return result;
        }
        return Base::initialize();
    }

    /// @brief Registers a subject ID to use for broadcasting to
    /// @param[in] subject_id Subject ID to use for broadcasting to
    Status registerSubjectId(const PortID subject_id) const noexcept
    {
        return udp_->registerPublication(subject_id, transport::TransferKindMessage);
    }

    /// @brief Wrapper to send multicast message
    /// @param[in] subject_id SubjectID to register to for sending messages
    /// @param[in] buffer the message payload
    /// @param[in] buffer_size size of the message
    Status broadcast(const PortID subject_id, const std::uint8_t* buffer, const std::size_t buffer_size)
    {
        cetl::pf20::span<const std::uint8_t> span_message{buffer, buffer_size};
        return udp_->broadcast(subject_id, span_message);
    }
};

}  // namespace udp
}  // namespace wrappers
}  // namespace libcyphal

#endif  // POSIX_LIBCYPHAL_WRAPPERS_UDP_BROADCASTER_HPP_INCLUDED
