/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Input session handler for broadcaster session

#ifndef POSIX_LIBCYPHAL_TRANSPORT_CAN_OUTPUT_SESSION_HPP_INCLUDED
#define POSIX_LIBCYPHAL_TRANSPORT_CAN_OUTPUT_SESSION_HPP_INCLUDED

#include <cstdint>
#include <cstring>
#include <libcyphal/transport/id_types.hpp>
#include <libcyphal/transport/can/session/message_publisher.hpp>
#include <libcyphal/transport/can/types.hpp>
#include <libcyphal/types/status.hpp>
#include "posix/libcyphal/transport/can/connection.hpp"

namespace libcyphal
{
namespace transport
{
namespace can
{
namespace session
{

/// @brief Used to store session information for CAN broadcasts
/// @todo Make this usable for service requests also
class PosixMessagePublisher : public MessagePublisher
{
public:
    /// @note based on IFNAMSIZ for posix systems
    static constexpr std::size_t MaximumInterfaceNameLength = 16;

    /// @brief Constructor
    /// @param[in] node_id Node id of local host
    /// @param[in] can_interface CAN Interface name
    PosixMessagePublisher(const NodeID node_id, const char* can_interface) noexcept
        : node_id_{node_id}
    {
        strncpy(can_interface_, can_interface, MaximumInterfaceNameLength);
    }

    /// @brief Destructor that cleans up posix socket connections
    virtual ~PosixMessagePublisher()
    {
        if (socket_fd_ != ClosedSocket)
        {
            int result = close(socket_fd_);
            assert(result != SocketFunctionError);
            (void)result;
            socket_fd_ = ClosedSocket;
        }
    }

    /// @brief Copy Constructor
    /// @param[in] other PosixMessagePublisher to copy from
    PosixMessagePublisher(const PosixMessagePublisher& other) noexcept
        : node_id_{other.node_id_}
        , socket_fd_{other.socket_fd_}
    {
        strncpy(can_interface_, other.can_interface_, MaximumInterfaceNameLength);
    }

    /// @brief Move Constructor
    /// @param[in] other PosixMessagePublisher to move from
    PosixMessagePublisher(PosixMessagePublisher&& other) noexcept
        : node_id_{other.node_id_}
        , socket_fd_{other.socket_fd_}
    {
        strncpy(can_interface_, other.can_interface_, MaximumInterfaceNameLength);
        memset(other.can_interface_, 0, MaximumInterfaceNameLength);
    }

    /// @brief Copy Assignment
    /// @param[in] other PosixMessagePublisher to copy from
    PosixMessagePublisher& operator=(const PosixMessagePublisher& other) noexcept
    {
        if (this != &other)
        {
            node_id_   = other.node_id_;
            socket_fd_ = other.socket_fd_;
            strncpy(can_interface_, other.can_interface_, MaximumInterfaceNameLength);
        }
        return *this;
    }

    /// @brief Move Assignment
    /// @param[in] other PosixMessagePublisher to move from
    PosixMessagePublisher& operator=(PosixMessagePublisher&& other) noexcept
    {
        if (this != &other)
        {
            node_id_   = other.node_id_;
            socket_fd_ = other.socket_fd_;
            strncpy(can_interface_, other.can_interface_, MaximumInterfaceNameLength);
            memset(other.can_interface_, 0, MaximumInterfaceNameLength);
        }
        return *this;
    }

    /// @brief Initializes the Session handler
    Status initialize() override
    {
        socket_fd_ = createSocket();
        if (socket_fd_ == ClosedSocket)
        {
            return ResultCode::Failure;
        }
        return initializeSocket(socket_fd_, can_interface_);
    }

    /// @brief Sets up everything needed to broadcast messages for a given subject id
    /// @note Current just uses one port and address to broadcast all messages
    /// @todo Investigate if multiple ports / address are desired per message / subject id
    /// @param[in] subject_id The subject id to broadcast on
    /// @param[in] frame The CAN Extended Frame type
    Status broadcast(PortID subject_id, const media::can::extended::Frame& frame) override
    {
        (void)subject_id;
        return transmitMessage(socket_fd_, frame);
    }

private:
    NodeID node_id_{CANARD_NODE_ID_UNSET};
    Socket socket_fd_{-1};
    char   can_interface_[MaximumInterfaceNameLength]{""};
};

}  // namespace session
}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // POSIX_LIBCYPHAL_TRANSPORT_CAN_OUTPUT_SESSION_HPP_INCLUDED
