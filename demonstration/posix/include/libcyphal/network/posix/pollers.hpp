/// @file
/// Contains IPoll implementations for POSIX systems.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
// cSpell: words sayin'

#ifndef LIBCYPHAL_NETWORK_POSIX_POLLERS_HPP_INCLUDED
#define LIBCYPHAL_NETWORK_POSIX_POLLERS_HPP_INCLUDED

#include "libcyphal/libcyphal.hpp"
#include "libcyphal/network/poll.hpp"
#include "libcyphal/network/posix/posix.hpp"
#include "libcyphal/network/posix/sockets.hpp"

namespace libcyphal
{
namespace network
{
namespace posix
{

/// Implements IPoll using posix poll.
/// A more optimized solution on Linux would be to use epoll but this version works on both Linux and OSx.
/// Just sayin'.
class PosixReadPoller : public IPoll
{
public:
    /// The maximum number of sockets that can be registered with this object. This is the maximum size of a Cyphal
    /// redundancy group as defined by section 1.4 of the specification.
    static constexpr std::size_t MaxSockets = 3U;

    virtual ~PosixReadPoller() = default;

    PosixReadPoller();
    PosixReadPoller(const PosixReadPoller&)            = delete;
    PosixReadPoller& operator=(const PosixReadPoller&) = delete;
    PosixReadPoller(PosixReadPoller&&);
    PosixReadPoller& operator=(PosixReadPoller&&);

    // +-----------------------------------------------------------------------+
    // | IPoll
    // +-----------------------------------------------------------------------+
    Status reserveRegistrarCapacity(std::size_t socketCount) override;

    Status registerSocket(ISocket* s) override;

    Status unregisterSocket(ISocket* s) noexcept override;

    Status clear() noexcept override;

    Status poll(SocketEventList& out_event_list, std::chrono::microseconds wait_for) override;

private:
    void     move(PosixReadPoller&& rhs) noexcept;
    ::nfds_t poll_fds_count_;
    ::pollfd poll_fds_[MaxSockets];

    /// index-to-index mapping between poll_fds and the socket that abstracts each.
    IPosixSocket* poll_fds_to_socket_map_[MaxSockets];
};

}  // namespace posix
}  // namespace network
}  // namespace libcyphal

#endif  // LIBCYPHAL_NETWORK_POSIX_POLLERS_HPP_INCLUDED
