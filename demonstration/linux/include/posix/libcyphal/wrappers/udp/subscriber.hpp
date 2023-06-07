/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Transport Interface Wrapper for receiving message over a UDP bus in POSIX

#ifndef POSIX_LIBCYPHAL_WRAPPERS_UDP_SUBSCRIBER_HPP_INCLUDED
#define POSIX_LIBCYPHAL_WRAPPERS_UDP_SUBSCRIBER_HPP_INCLUDED

#include <libcyphal/transport/id_types.hpp>
#include <libcyphal/transport/ip/v4/address.hpp>
#include <libcyphal/transport/ip/v4/types.hpp>
#include <libcyphal/transport/listener.hpp>
#include <libcyphal/transport/metadata.hpp>
#include <libcyphal/types/status.hpp>
#include "posix/libcyphal/wrappers/udp/base.hpp"

namespace libcyphal
{
namespace wrappers
{
namespace udp
{

/// @brief UDPSubscriber is a wrapper around the tasks needed to receive messages
class Subscriber final : public Base
{
public:
    transport::Listener& listener_;

    /// @brief Constructor for UDPSubscriber which is a wrapper around the tasks needed to receive messages
    /// @param[in] ip_address Local IP Address
    /// @param[in] node_id The desired NodeID of the Transport
    /// @param[in] listener Listener object providing custom triggers by the user
    Subscriber(const transport::ip::v4::Address ip_address, const NodeID node_id, transport::Listener& listener)
        : Base(ip_address, node_id)
        , listener_{listener}
    {}

    /// Destructor
    ~Subscriber() = default;

    /// @brief Initializes everything needed to receive frames
    Status initialize() override
    {
        Status result{};
        result = interface_.initializeInput();
        if (result.isFailure())
        {
            return result;
        }
        return Base::initialize();
    }

    /// @brief Register a subject ID for receiving messages on
    /// @param[in] subject_id Subject ID to subscribe to
    Status registerSubjectId(const PortID subject_id) noexcept
    {
        Status result{};
        result = interface_.setupReceiver(subject_id);
        if (result.isFailure())
        {
            return result;
        }
        return udp_->registerSubscription(subject_id, transport::TransferKindMessage);
    }

    /// @brief Receives any incoming frames and triggers the listener as needed
    Status receive() const
    {
        return udp_->processIncomingTransfers(listener_);
    }
};

}  // namespace udp
}  // namespace wrappers
}  // namespace libcyphal

#endif  // POSIX_LIBCYPHAL_WRAPPERS_UDP_SUBSCRIBER_HPP_INCLUDED
