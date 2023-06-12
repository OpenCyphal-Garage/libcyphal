/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Transport Interface Wrapper for sending message over a UDP bus in POSIX

#ifndef POSIX_LIBCYPHAL_WRAPPERS_CAN_BROADCASTER_HPP_INCLUDED
#define POSIX_LIBCYPHAL_WRAPPERS_CAN_BROADCASTER_HPP_INCLUDED

#include <cstdint>
#include <cstddef>
#include <libcyphal/transport/id_types.hpp>
#include <libcyphal/transport/metadata.hpp>
#include <libcyphal/types/status.hpp>
#include "posix/libcyphal/wrappers/can/base.hpp"
#include "cetl/pf20/span.hpp"

namespace libcyphal
{
namespace wrappers
{
namespace can
{

/// @brief CANBroadcaster is a wrapper around the tasks needed to send messages
class Broadcaster final : public Base
{
public:
    /// @brief Constructor for CANBroadcaster which is a wrapper around the tasks needed to send messages
    /// @param[in] can_interface CAN Interface to use
    /// @param[in] node_id The desired NodeID of the Transport
    Broadcaster(const char* can_interface, const NodeID node_id, cetl::pf17::pmr::memory_resource* resource) noexcept
        : Base(can_interface, node_id, resource)
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

    Status registerSubjectId(const PortID subject_id) const noexcept
    {
        return can_->registerPublication(subject_id, transport::TransferKindMessage);
    }

    /// @brief Wrapper to send multicast message
    /// @param[in] subject_id SubjectID to register to for sending messages
    /// @param[in] buffer the message payload
    /// @param[in] buffer_size size of the message
    Status broadcast(const PortID subject_id, const std::uint8_t* buffer, std::size_t buffer_size)
    {
        cetl::pf20::span<const std::uint8_t> span_message{buffer, buffer_size};
        return can_->broadcast(subject_id, span_message);
    }
};

}  // namespace can
}  // namespace wrappers
}  // namespace libcyphal

#endif  // POSIX_LIBCYPHAL_WRAPPERS_CAN_BROADCASTER_HPP_INCLUDED
