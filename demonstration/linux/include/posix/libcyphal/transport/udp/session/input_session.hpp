/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Input session handler for message and service receiver sessions

#ifndef POSIX_LIBCYPHAL_TRANSPORT_UDP_INPUT_SESSION_HPP_INCLUDED
#define POSIX_LIBCYPHAL_TRANSPORT_UDP_INPUT_SESSION_HPP_INCLUDED

#include <cassert>
#include <cstdint>
#include <array>
#include <libcyphal/transport/id_types.hpp>
#include <libcyphal/transport/ip/v4/types.hpp>
#include <libcyphal/transport/udp/cyphal_udp_transport.hpp>
#include <libcyphal/transport/udp/interface.hpp>
#include <libcyphal/transport/udp/transport.hpp>
#include <libcyphal/transport/udp/session/input_session.hpp>
#include <libcyphal/transport/udp/session/specifier.hpp>
#include <libcyphal/types/status.hpp>
#include "posix/libcyphal/transport/ip/v4/connection.hpp"
#include "posix/libcyphal/transport/ip/v4/properties.hpp"

#include "cetl/variable_length_array.hpp"
#include "cetl/pf17/memory_resource.hpp"

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
        , storage_{}
        , resource_{storage_.data(), storage_.size()}
        , data_{cetl::pf17::pmr::polymorphic_allocator<Specifier>{&resource_}}
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
                (void) result;
                data.socket_fd = ip::v4::ClosedSocket;
            }
        }
    }

    /// @brief Copy Constructor
    /// @param[in] other PosixInputSession to copy from
    PosixInputSession(const PosixInputSession& other) noexcept
        : node_id_{other.node_id_}
        , local_address_{other.local_address_}
        , storage_{}
        , resource_{storage_.data(), storage_.size()}
        , data_{other.data_, &resource_}
    {
    }

    /// @brief Move Constructor
    /// @param[in] other PosixInputSession to move from
    PosixInputSession(PosixInputSession&& other) noexcept
        : node_id_{other.node_id_}
        , local_address_{other.local_address_}
        , storage_{}
        , resource_{storage_.data(), storage_.size()}
        , data_{other.data_, &resource_}
    {
        for (std::uint16_t i = 0; i < other.data_.size(); i++)
        {
            other.data_.pop_back();
        }
    }

    /// @brief Copy Assignment
    /// @param[in] other PosixInputSession to copy from
    PosixInputSession& operator=(const PosixInputSession& other) noexcept
    {
        if (this != &other)
        {
            node_id_       = other.node_id_;
            local_address_ = other.local_address_;
            data_          = other.data_;
        }
        return *this;
    }

    /// @brief Move Assignment
    /// @param[in] other PosixInputSession to move from
    PosixInputSession& operator=(PosixInputSession&& other) noexcept
    {
        if (this != &other)
        {
            node_id_       = other.node_id_;
            local_address_ = other.local_address_;
            data_          = std::move(other.data_);
            for (std::uint16_t i = 0; i < other.data_.size(); i++)
            {
                other.data_.pop_back();
            }
        }
        return *this;
    }

    /// @brief Initializes the Session handler
    Status initialize() override
    {
        return ResultCode::Success;
    }

    /// @brief Sets up everything needed to receive messages on a given subject ID
    /// @note Creates a new Posix Socket per Subject ID with a unique IP Address
    /// @param[in] subject_id The subject id to listen on
    Status setupMessageReceiver(const PortID subject_id) noexcept override
    {
        Status    result{};
        Specifier data{};
        // TODO OVPG-3432 Update this function to use the IP address calculated by libudpard instead of recalculating it here
        data.target_address = ip::v4::getMulticastAddressFromSubjectId(subject_id);
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

    /// @brief Sets up everything needed to receive a request or response on the local Node
    /// @note Creates a new Posix Socket per local Node ID with a unique IP Address
    Status setupServiceReceiver(NodeID node_id) noexcept override
    {
        Status    result{};
        Specifier data{};

        if (node_id != node_id_) {
            return ResultCode::BadArgument;
        }
        // TODO OVPG-3432 Update this function to use the IP address calculated by libudpard instead of recalculating it here
        data.target_address = ip::v4::getMulticastAddressFromServiceNodeId(node_id_);
        data.node_id        = node_id_;
        data.socket_port    = ip::v4::BroadcastPort;
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

    /// @brief Receives all transfers for all registered Port IDs
    /// @param[in] receiver Transport receiver that makes calls to libudpard
    Status receive(Interface::Receiver& receiver) noexcept override
    {
        std::size_t       message_size = media::udp::MaximumMTUBytes;
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
    NodeID                                                    node_id_{UDPARD_NODE_ID_UNSET};
    ip::v4::Address                                           local_address_{};
    std::array<Specifier, MaxNumberOfSubscriptionRecords>     storage_;
    cetl::pf17::pmr::deviant::basic_monotonic_buffer_resource resource_;
    cetl::VariableLengthArray<Specifier, cetl::pf17::pmr::polymorphic_allocator<Specifier>> data_;
};

}  // namespace session
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // POSIX_LIBCYPHAL_TRANSPORT_UDP_INPUT_SESSION_HPP_INCLUDED
