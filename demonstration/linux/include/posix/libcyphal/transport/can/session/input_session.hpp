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
#include "libcyphal/transport/can/session/input_session.hpp"
#include <libcyphal/transport/can/can_interface.hpp>
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
class PosixInputSession : public InputSession
{
public:
    /// @note based on IFNAMSIZ for posix systems
    static constexpr std::size_t MaximumInterfaceNameLength = 16;

    PosixInputSession() = delete;

    /// @brief Constructor
    /// @param[in] node_id Node id of local host
    /// @param[in] can_interface Can interface name (example: can0)
    PosixInputSession(const NodeID node_id, const char* can_interface) noexcept
        : node_id_{node_id}
    {
        strncpy(can_interface_, can_interface, MaximumInterfaceNameLength);
    }

    /// @brief Destructor that cleans up posix socket connections
    virtual ~PosixInputSession()
    {
        if (socket_fd_ != ClosedSocket)
        {
            int result = close(socket_fd_);
            assert(result != SocketFunctionError);
            socket_fd_ = ClosedSocket;
        }
    }

    /// @brief Copy Constructor
    /// @param[in] other PosixInputSession to copy from
    PosixInputSession(const PosixInputSession& other) noexcept
        : node_id_{other.node_id_}
    {
        socket_fd_ = other.socket_fd_;
        strncpy(can_interface_, other.can_interface_, MaximumInterfaceNameLength);
    }

    /// @brief Move Constructor
    /// @param[in] other PosixInputSession to move from
    PosixInputSession(PosixInputSession&& other) noexcept
        : node_id_{other.node_id_}
    {
        socket_fd_ = other.socket_fd_;
        strncpy(can_interface_, other.can_interface_, MaximumInterfaceNameLength);
        memset(other.can_interface_, 0, MaximumInterfaceNameLength);
    }

    /// @brief Copy Assignment
    /// @param[in] other PosixInputSession to copy from
    PosixInputSession& operator=(const PosixInputSession& other) noexcept
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
    /// @param[in] other PosixInputSession to move from
    PosixInputSession& operator=(PosixInputSession&& other) noexcept
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

    /// @brief Receives a frame for all registered Port IDs
    /// @param[in] receiver Transport receiver that makes calls to libcanard
    Status receiveFrames(NetworkInterface::Receiver& receiver) override
    {
        Status result{};
        media::can::extended::Frame frame{};
        result = receiveFrame(socket_fd_, frame);
        if (result.isSuccess())
        {
            receiver.onReceiveFrame(frame);
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
