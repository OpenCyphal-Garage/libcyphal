/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Input session handler for subscriber session

#ifndef POSIX_LIBCYPHAL_TRANSPORT_UDP_INPUT_SESSION_HPP_INCLUDED
#define POSIX_LIBCYPHAL_TRANSPORT_UDP_INPUT_SESSION_HPP_INCLUDED

#include <cassert>
#include <cstdint>
#include <libcyphal/transport/id_types.hpp>
#include <libcyphal/transport/ip/v4/types.hpp>
#include <libcyphal/transport/udp/cyphal_udp_transport.hpp>
#include <libcyphal/transport/udp/interface.hpp>
#include <libcyphal/transport/udp/transport.hpp>
#include <libcyphal/transport/udp/session/message_subscriber.hpp>
#include <libcyphal/transport/udp/session/specifier.hpp>
#include <libcyphal/types/status.hpp>
#include "posix/libcyphal/transport/ip/v4/connection.hpp"
#include "posix/libcyphal/transport/ip/v4/properties.hpp"

namespace libcyphal
{
namespace transport
{
namespace udp
{
namespace session
{

/// @brief Used to store session information for UDP subscriptions
/// @todo Make this usable for service requests also
class PosixMessageSubscriber final : public MessageSubscriber
{
public:
    PosixMessageSubscriber() = delete;

    /// @brief Constructor
    /// @param[in] node_id Node id of local host
    /// @param[in] local_address Local ip address
    PosixMessageSubscriber(const NodeID node_id, const ip::v4::Address local_address) noexcept
        : node_id_{node_id}
        , local_address_{local_address}
    {}

    /// @brief Destructor that cleans up posix socket connections
    ~PosixMessageSubscriber()
    {
        for (Specifier& data : data_)
        {
            if (data.socket_fd != ip::v4::ClosedSocket)
            {
                int result = close(data.socket_fd);
                assert(result != ip::v4::SocketFunctionError);
                data.socket_fd = ip::v4::ClosedSocket;
            }
        }
    }

    /// @brief Copy Constructor
    /// @param[in] other PosixMessageSubscriber to copy from
    PosixMessageSubscriber(const PosixMessageSubscriber& other) noexcept
        : node_id_{other.node_id_}
        , local_address_{other.local_address_}
        , data_{List<Specifier, MaxNumberOfSubscriptionRecords>(other.data_)}
    {}

    /// @brief Move Constructor
    /// @param[in] other PosixMessageSubscriber to move from
    PosixMessageSubscriber(PosixMessageSubscriber&& other) noexcept
        : node_id_{other.node_id_}
        , local_address_{other.local_address_}
        , data_{List<Specifier, MaxNumberOfSubscriptionRecords>(other.data_)}
    {
        for (std::uint16_t i = 0; i < other.data_.size(); i++)
        {
            other.data_.dismiss_back();
        }
    }

    /// @brief Copy Assignment
    /// @param[in] other PosixMessageSubscriber to copy from
    PosixMessageSubscriber& operator=(const PosixMessageSubscriber& other) noexcept
    {
        if (this != &other)
        {
            node_id_       = other.node_id_;
            local_address_ = other.local_address_;
            data_ = List<Specifier, MaxNumberOfSubscriptionRecords>(other.data_);
        }
        return *this;
    }

    /// @brief Move Assignment
    /// @param[in] other PosixMessageSubscriber to move from
    PosixMessageSubscriber& operator=(PosixMessageSubscriber&& other) noexcept
    {
        if (this != &other)
        {
            node_id_       = other.node_id_;
            local_address_ = other.local_address_;
            data_ = List<Specifier, MaxNumberOfSubscriptionRecords>(other.data_);
            for (std::uint16_t i = 0; i < other.data_.size(); i++)
            {
                other.data_.dismiss_back();
            }
        }
        return *this;
    }

    /// @brief Initializes the Session handler
    Status initialize() override
    {
        return ResultCode::Success;
    }

    /// @brief Sets up everything needed to receive messages on a given subject id
    /// @note Creates a new Posix Socket per Subject ID with a unique IP Address
    /// @param[in] subject_id The subject id to listen on
    Status setupReceiver(const PortID subject_id) noexcept override
    {
        Status    result{};
        Specifier data{};
        data.target_address = ip::v4::getBroadcastAddressFromSubjectId(subject_id);
        data.node_id        = node_id_;
        data.socket_port    = ip::v4::BroadcastPort;
        data.port_id        = subject_id;
        data.socket_fd      = ip::v4::createSocket();
        if (data.socket_fd == ip::v4::ClosedSocket)
        {
            return ResultCode::Failure;
        }
        data_.emplace_back(data);
        result = ip::v4::bindToSocket(data.socket_fd, data.target_address, data.socket_port);
        if (result.isFailure())
        {
            return result;
        }
        return ip::v4::setJoinMulticastGroup(data.socket_fd,
                                             data.target_address.asInteger(),
                                             local_address_.asInteger());
    }

    /// @brief Receives all messages for subscribed subject ids
    /// @param[in] receiver Transport receiver that makes calls to libudpard
    Status receive(Interface::Receiver& receiver) noexcept override
    {
        std::size_t message_size = media::udp::MaximumMTUBytes;
        media::udp::Frame frame{};

        // This loop iterates over the sockets for all the port IDs we have subscribed to.
        // If we successfully receive data for at least one port ID, we should return Success.
        // If we don't receive data for any port ID, we should return Failure.
        Status overall_result{ResultCode::Failure};
        for (Specifier& data : data_)
        {
            memset(frame.data_, 0, message_size);
            frame.data_length_ = message_size;

            Status result = ip::v4::receiveMessage(data.socket_fd, data.target_address, data.socket_port, frame);
            if (result.isSuccess())
            {
                memcpy(&frame.header_, frame.data_, sizeof(frame.header_));  // NOLINT
                receiver.onReceive(frame);
                overall_result.setResult(result.getResultCode());
            }
        }
        return overall_result;
    }

private:
    NodeID node_id_{UDPARD_NODE_ID_UNSET};
    ip::v4::Address local_address_{};
    List<Specifier, MaxNumberOfSubscriptionRecords> data_;
};

}  // namespace session
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // POSIX_LIBCYPHAL_TRANSPORT_UDP_INPUT_SESSION_HPP_INCLUDED
