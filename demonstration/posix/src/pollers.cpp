/// @file
/// Contains IPoll implementations for POSIX systems.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "libcyphal/network/posix/pollers.hpp"
#include "libcyphal/janky.hpp"

namespace libcyphal
{
namespace network
{
namespace posix
{

void PosixReadPoller::move(PosixReadPoller&& rhs) noexcept
{
    poll_fds_count_     = rhs.poll_fds_count_;
    rhs.poll_fds_count_ = 0;
    ::memcpy(poll_fds_, rhs.poll_fds_, sizeof(::pollfd) * poll_fds_count_);
    ::memcpy(poll_fds_to_socket_map_, rhs.poll_fds_to_socket_map_, sizeof(IPosixSocket) * poll_fds_count_);

    // Zero out rhs to ensure any core dumps or other debug tools don't get confused by stale values.
    ::memset(rhs.poll_fds_, 0, sizeof(::pollfd) * MaxSockets);
    ::memset(rhs.poll_fds_to_socket_map_, 0, sizeof(::pollfd) * MaxSockets);
}

PosixReadPoller::PosixReadPoller()
    : poll_fds_count_{0}
    , poll_fds_{}
    , poll_fds_to_socket_map_{}
{
    ::memset(poll_fds_to_socket_map_, 0, sizeof(IPosixSocket*) * MaxSockets);
    ::memset(poll_fds_, 0, sizeof(::pollfd) * MaxSockets);
}

PosixReadPoller::PosixReadPoller(PosixReadPoller&& rhs)
    : poll_fds_count_{0}
    , poll_fds_{}
    , poll_fds_to_socket_map_{}
{
    move(std::move(rhs));
}

PosixReadPoller& PosixReadPoller::operator=(PosixReadPoller&& rhs)
{
    move(std::move(rhs));
    return *this;
}

// +---------------------------------------------------------------------------+
// | IPoll
// +---------------------------------------------------------------------------+

Status PosixReadPoller::reserveRegistrarCapacity(std::size_t socketCount)
{
    // This implementation uses static memory within the object so we are only confirming that we have enough
    // static memory.
    return (socketCount <= MaxSockets) ? ResultCode::Success : ResultCode::MemoryError;
}

Status PosixReadPoller::registerSocket(ISocket* s)
{
    if (nullptr == s)
    {
        return ResultCode::InvalidArgumentError;
    }

    IPosixSocket* as_posix = janky::polymorphic_type_traits::safe_downcast<IPosixSocket>(*s);

    if (nullptr == as_posix)
    {
        return ResultCode::InvalidArgumentError;
    }

    if (poll_fds_count_ == MaxSockets)
    {
        return ResultCode::MemoryError;
    }

    Status socket_status = s->getStatus();
    if (!socket_status)
    {
        return Status{socket_status.result, 0x88};
    }

    // We expect setting up a new poll object to be rare enough that we're trading
    // the simplicity of a flat memory structure for the runtime performance of something
    // like a hash set. This looks ugly but it makes the poll logic very simple which
    // is what we are optimizing for.
    for (std::size_t i = 0; i < MaxSockets; ++i)
    {
        if (janky::polymorphic_type_traits::isEqual(poll_fds_to_socket_map_[i], as_posix))
        {
            // Already registered.
            return ResultCode::Success;
        }
    }
    poll_fds_to_socket_map_[poll_fds_count_] = as_posix;
    ::pollfd& open_slot_fd                   = poll_fds_[poll_fds_count_++];

    open_slot_fd.fd      = as_posix->getSocketFd();
    open_slot_fd.events  = (POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI);
    open_slot_fd.revents = 0;
    return ResultCode::Success;
}

Status PosixReadPoller::unregisterSocket(ISocket* s) noexcept
{
    if (nullptr == s)
    {
        return ResultCode::InvalidArgumentError;
    }

    IPosixSocket* as_posix = janky::polymorphic_type_traits::safe_downcast<IPosixSocket>(*s);

    if (nullptr == as_posix)
    {
        return ResultCode::InvalidArgumentError;
    }

    const ::nfds_t before_unregister_count = poll_fds_count_;
    ::nfds_t       i                       = 0;
    for (; i < before_unregister_count; ++i)
    {
        if (janky::polymorphic_type_traits::isEqual(poll_fds_to_socket_map_[i], as_posix))
        {
            poll_fds_to_socket_map_[i] = nullptr;
            poll_fds_[i].fd            = -1;
            --poll_fds_count_;
            break;
        }
    }
    // compact the rest of the array to remove the space since ::poll can't handle a sparse array.
    if (poll_fds_count_ < before_unregister_count)
    {
        ::memmove(&poll_fds_[i], &poll_fds_[i + 1], sizeof(::nfds_t) * before_unregister_count - poll_fds_count_);
        ::memmove(&poll_fds_to_socket_map_[i],
                  &poll_fds_to_socket_map_[i + 1],
                  sizeof(IPosixSocket*) * before_unregister_count - poll_fds_count_);
    }
    return ResultCode::Success;
}

Status PosixReadPoller::clear() noexcept
{
    poll_fds_count_ = 0;
    ::memset(poll_fds_to_socket_map_, 0, sizeof(IPosixSocket*) * MaxSockets);
    ::memset(poll_fds_, 0, sizeof(::pollfd) * MaxSockets);
    return ResultCode::Success;
}

Status PosixReadPoller::poll(SocketEventList& out_event_list, std::chrono::microseconds wait_for_micros)
{
    int poll_result = ::poll(poll_fds_, poll_fds_count_, static_cast<int>(wait_for_micros.count()) / 1000);
    if (poll_result == 0)
    {
        return ResultCode::Timeout;
    }
    else if (poll_result < 0)
    {
        return makeNetworkStatusWithErrno(ResultCode::NetworkSystemError, errno);
    }
    // TODO: https://github.com/OpenCyphal/CETL/issues/44 never allow exceptions in CETL.
    out_event_list.reserve(static_cast<std::size_t>(poll_result));
    if (out_event_list.capacity() < static_cast<std::size_t>(poll_result))
    {
        return ResultCode::MemoryError;
    }
    for (::nfds_t i = 0; i < poll_fds_count_; ++i)
    {
        if (poll_fds_[i].revents)
        {
            out_event_list.push_back(poll_fds_to_socket_map_[i]);
        }
    }
    return ResultCode::Success;
}

}  // namespace posix
}  // namespace network
}  // namespace libcyphal
