/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Input session handler for message and service receiver sessions

#ifndef POSIX_LIBCYPHAL_TRANSPORT_UDP_INPUT_SESSION_HPP_INCLUDED
#define POSIX_LIBCYPHAL_TRANSPORT_UDP_INPUT_SESSION_HPP_INCLUDED

#include <cassert>
#include <cstdint>
#include <libcyphal/transport/id_types.hpp>
#include <libcyphal/transport/ip/v4/types.hpp>
#include <libcyphal/transport/udp/cyphal_udp_transport.hpp>
#include <libcyphal/transport/udp/udp_interface.hpp>
#include <libcyphal/transport/udp/session/input_session.hpp>
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
class PosixInputSession final : public InputSession
{
public:
    PosixInputSession() = delete;

    /// @brief Constructor
    /// @param[in] node_id Node id of local host
    /// @param[in] local_address Local ip address
    PosixInputSession(const NodeID node_id, const ip::v4::Address local_address) noexcept
        : node_id_{node_id}
        , local_address_{local_address}
    {
    }

    /// @brief Destructor that cleans up posix socket connections
    virtual ~PosixInputSession()
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
    /// @param[in] other PosixInputSession to copy from
    PosixInputSession(const PosixInputSession& other) noexcept
        : node_id_{other.node_id_}
        , local_address_{other.local_address_}
        , service_receiver_initialized_{other.service_receiver_initialized_}
        , data_{List<Specifier, MaxNumberOfSubscriptionRecords>(other.data_)}
    {
    }

    /// @brief Move Constructor
    /// @param[in] other PosixInputSession to move from
    PosixInputSession(PosixInputSession&& other) noexcept
        : node_id_{other.node_id_}
        , local_address_{other.local_address_}
        , service_receiver_initialized_{other.service_receiver_initialized_}
        , data_{List<Specifier, MaxNumberOfSubscriptionRecords>(other.data_)}
    {
        for (std::uint16_t i = 0; i < other.data_.size(); i++)
        {
            other.data_.dismiss_back();
        }
    }

    /// @brief Copy Assignment
    /// @param[in] other PosixInputSession to copy from
    PosixInputSession& operator=(const PosixInputSession& other) noexcept
    {
        if (this != &other)
        {
            node_id_                      = other.node_id_;
            local_address_                = other.local_address_;
            service_receiver_initialized_ = other.service_receiver_initialized_;
            data_                         = List<Specifier, MaxNumberOfSubscriptionRecords>(other.data_);
        }
        return *this;
    }

    /// @brief Move Operator
    /// @param[in] other PosixInputSession to move from
    PosixInputSession& operator=(PosixInputSession&& other) noexcept
    {
        if (this != &other)
        {
            node_id_                      = other.node_id_;
            local_address_                = other.local_address_;
            service_receiver_initialized_ = other.service_receiver_initialized_;
            data_                         = List<Specifier, MaxNumberOfSubscriptionRecords>(other.data_);
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

    Status initializeSocket(Specifier& specifier)
    {
        Status result = ip::v4::bindToSocket(specifier.socket_fd, specifier.target_address, specifier.socket_port);
        if (result.isFailure())
        {
            return result;
        }

        return ip::v4::setJoinMulticastGroup(specifier.socket_fd,
                                             specifier.target_address.asInteger(),
                                             local_address_.asInteger());
    }

    /// @brief Sets up everything needed to receive messages on a given subject ID
    /// @note Creates a new Posix Socket per Subject ID with a unique IP Address
    /// @param[in] subject_id The subject id to listen on
    Status setupMessageReceiver(const PortID subject_id) noexcept override
    {
        Specifier message_specifier{};
        // TODO OVPG-3432 Update this function to use the IP address calculated by libudpard instead of recalculating it
        // here
        message_specifier.target_address = ip::v4::getMulticastAddressFromSubjectId(subject_id);
        message_specifier.node_id        = node_id_;
        message_specifier.socket_port    = ip::v4::BroadcastPort;
        message_specifier.port_id        = subject_id;
        message_specifier.socket_fd      = ip::v4::createSocket();
        if (message_specifier.socket_fd == ip::v4::ClosedSocket)
        {
            return ResultCode::Failure;
        }
        data_.emplace_back(message_specifier);

        return initializeSocket(message_specifier);
    }

    /// @brief Sets up everything needed to receive a request or response on the local Node
    /// @note Creates a new Posix Socket per local Node ID with a unique IP Address
    /// @note This only needs to be called once since all Requests and Responses will be received
    ///        on the same multicast address. This is because the destination multicast address
    ///        is calculated using the local Node ID. So, if an application's Node ID is 44,
    //         all requests and responses will be received on 239.1.0.44.
    /// @param[in] node_id The local Node ID
    Status setupServiceReceiver(NodeID node_id) noexcept override
    {
        if (service_receiver_initialized_)
        {
            return ResultCode::SuccessNothing;
        }

        if (node_id != node_id_)
        {
            return ResultCode::BadArgument;
        }

        Specifier service_specifier{};
        // TODO OVPG-3432 Update this function to use the IP address calculated by libudpard instead of recalculating it
        // here
        service_specifier.target_address = ip::v4::getMulticastAddressFromServiceNodeId(node_id_);
        service_specifier.node_id        = node_id_;
        service_specifier.socket_port    = ip::v4::BroadcastPort;
        service_specifier.socket_fd      = ip::v4::createSocket();
        if (service_specifier.socket_fd == ip::v4::ClosedSocket)
        {
            return ResultCode::Failure;
        }

        Status status = initializeSocket(service_specifier);
        if (status.isFailure())
        {
            return status;
        }

        data_.emplace_back(service_specifier);
        service_receiver_initialized_ = true;
        return status;
    }

    /// @brief Receives a frame for all registered Port IDs (messages, requests, and responses)
    /// @param[in] receiver Transport receiver that makes calls to libudpard
    /// @return Status of receiving data
    Status receiveFrames(NetworkInterface::Receiver& receiver) noexcept override
    {
        std::size_t       frame_size = media::udp::MaximumMTUBytes;
        media::udp::Frame frame{};

        // This loop iterates over the sockets for all the port IDs we have subscribed to.
        // If we successfully receive data for at least one port ID, we should return Success.
        // If we don't receive data for any port ID, we should return the last received failure code.
        Status overall_result{ResultCode::Failure};
        Status result{ResultCode::Failure};
        for (Specifier& data : data_)
        {
            memset(frame.data_, 0, frame_size);
            frame.data_length_ = frame_size;

            result = ip::v4::receiveFrame(data.socket_fd, data.target_address, data.socket_port, frame);
            if (result.isSuccess())
            {
                memcpy(&frame.header_, frame.data_, sizeof(frame.header_));  // NOLINT
                receiver.onReceiveFrame(frame);
                overall_result.setResult(result.getResultCode());
            }
        }

        if (overall_result.isFailure())
        {
            // Return the last known failure
            return result;
        }

        return overall_result;
    }

private:
    NodeID                                          node_id_{UDPARD_NODE_ID_UNSET};
    ip::v4::Address                                 local_address_{};
    bool                                            service_receiver_initialized_{false};
    List<Specifier, MaxNumberOfSubscriptionRecords> data_;
};

}  // namespace session
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // POSIX_LIBCYPHAL_TRANSPORT_UDP_INPUT_SESSION_HPP_INCLUDED
