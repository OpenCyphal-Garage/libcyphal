/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Input session handler for subscriber session

#ifndef POSIX_LIBCYPHAL_TRANSPORT_CAN_INPUT_SESSION_HPP_INCLUDED
#define POSIX_LIBCYPHAL_TRANSPORT_CAN_INPUT_SESSION_HPP_INCLUDED

#include <cstdint>
#include <cstring>
#include <libcyphal/media/can/frame.hpp>
#include <libcyphal/transport/id_types.hpp>
#include <libcyphal/transport/can/cyphal_can_transport.hpp>
#include <libcyphal/transport/can/session/message_publisher.hpp>
#include <libcyphal/transport/can/session/message_subscriber.hpp>
#include <libcyphal/transport/can/interface.hpp>
#include <libcyphal/transport/can/transport.hpp>
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

/// @brief Used to store session information for CAN subscriptions
/// @todo Make this usable for service requests also
class PosixMessageSubscriber : public MessageSubscriber
{
public:
    /// @note based on IFNAMSIZ for posix systems
    static constexpr std::size_t MaximumInterfaceNameLength = 16;

    PosixMessageSubscriber() = delete;

    /// @brief Constructor
    /// @param[in] node_id Node id of local host
    /// @param[in] can_interface Can interface name (example: can0)
    PosixMessageSubscriber(const NodeID node_id, const char* can_interface) noexcept
        : node_id_{node_id}
    {
        strncpy(can_interface_, can_interface, MaximumInterfaceNameLength);
    }

    /// @brief Destructor that cleans up posix socket connections
    virtual ~PosixMessageSubscriber()
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
    /// @param[in] other PosixMessageSubscriber to copy from
    PosixMessageSubscriber(const PosixMessageSubscriber& other) noexcept
        : node_id_{other.node_id_}
    {
        socket_fd_ = other.socket_fd_;
        strncpy(can_interface_, other.can_interface_, MaximumInterfaceNameLength);
    }

    /// @brief Move Constructor
    /// @param[in] other PosixMessageSubscriber to move from
    PosixMessageSubscriber(PosixMessageSubscriber&& other) noexcept
        : node_id_{other.node_id_}
    {
        socket_fd_ = other.socket_fd_;
        strncpy(can_interface_, other.can_interface_, MaximumInterfaceNameLength);
        memset(other.can_interface_, 0, MaximumInterfaceNameLength);
    }

    /// @brief Copy Assignment
    /// @param[in] other PosixMessageSubscriber to copy from
    PosixMessageSubscriber& operator=(const PosixMessageSubscriber& other) noexcept
    {
        if (this != &other)
        {
            socket_fd_ = other.socket_fd_;
            node_id_   = other.node_id_;
            strncpy(can_interface_, other.can_interface_, MaximumInterfaceNameLength);
        }
        return *this;
    }

    /// @brief Move Assignment
    /// @param[in] other PosixMessageSubscriber to move from
    PosixMessageSubscriber& operator=(PosixMessageSubscriber&& other) noexcept
    {
        if (this != &other)
        {
            socket_fd_ = other.socket_fd_;
            node_id_   = other.node_id_;
            strncpy(can_interface_, other.can_interface_, MaximumInterfaceNameLength);
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

    /// @brief Receives all messages for subscribed subject ids
    /// @param[in] receiver Transport receiver that makes calls to libcanard
    Status receive(Interface::Receiver& receiver) override
    {
        Status result{};
        media::can::extended::Frame frame{};
        result = receiveMessage(socket_fd_, frame);
        if (result.isSuccess())
        {
            receiver.onReceive(frame);
        }
        return result;
    }

private:
    Socket       socket_fd_{-1};
    NodeID       node_id_{CANARD_NODE_ID_UNSET};
    char         can_interface_[MaximumInterfaceNameLength]{""};
};

}  // namespace session
}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // POSIX_LIBCYPHAL_TRANSPORT_CAN_INPUT_SESSION_HPP_INCLUDED
