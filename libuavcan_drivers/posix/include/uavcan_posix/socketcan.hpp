/*
 * Copyright (C) 2014-2015 Pavel Kirienko <pavel.kirienko@gmail.com>
 *                         Ilia Sheremet <illia.sheremet@gmail.com>
 * Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>

#include <uavcan/uavcan.hpp>
#include <uavcan_posix/system_clock.hpp>
#include <uavcan_posix/exception.hpp>
#include <uavcan_posix/socketcan_iface.hpp>

namespace uavcan_posix
{

/**
 * Multiplexing container for multiple SocketCAN sockets.
 * Uses poll() for multiplexing.
 *
 * When an interface becomes down/disconnected while the node is running,
 * the driver will silently exclude it from the IO loop and continue to run on the remaining interfaces.
 * When all interfaces become down/disconnected, the driver will throw @ref AllIfacesDownException
 * from @ref SocketCanDriver::select().
 * Whether a certain interface is down can be checked with @ref SocketCanDriver::isIfaceDown().
 */
class SocketCanDriver : public uavcan::ICanDriver
{
    class IfaceWrapper : public SocketCanIface
    {
        bool down_ = false;

    public:
        IfaceWrapper(const uavcan_posix::ISystemClock& clock, int fd) : SocketCanIface(clock, fd) { }

        void updateDownStatusFromPollResult(const ::pollfd& pfd)
        {
            UAVCAN_ASSERT(pfd.fd == this->getFileDescriptor());
            if (!down_ && (pfd.revents & POLLERR))
            {
                int error = 0;
                ::socklen_t errlen = sizeof(error);
                (void)::getsockopt(pfd.fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<void*>(&error), &errlen);

                down_ = error == ENETDOWN || error == ENODEV;

                UAVCAN_TRACE("SocketCAN", "Iface %d is dead; error %d", this->getFileDescriptor(), error);
            }
        }

        bool isDown() const { return down_; }
    };

    const uavcan_posix::ISystemClock& clock_;
    std::vector<std::unique_ptr<IfaceWrapper>> ifaces_;

public:
    /**
     * Reference to the clock object shall remain valid.
     */
    explicit SocketCanDriver(const uavcan_posix::ISystemClock& clock)
        : clock_(clock)
    {
        ifaces_.reserve(uavcan::MaxCanIfaces);
    }

    /**
     * This function may return before deadline expiration even if no requested IO operations become possible.
     * This behavior makes implementation way simpler, and it is OK since libuavcan can properly handle such
     * early returns.
     * Also it can return more events than were originally requested by uavcan, which is also acceptable.
     */
    std::int16_t select(uavcan::CanSelectMasks& inout_masks,
                        const uavcan::CanFrame* (&)[uavcan::MaxCanIfaces],
                        uavcan::MonotonicTime blocking_deadline) override
    {
        // Detecting whether we need to block at all
        bool need_block = (inout_masks.write == 0);    // Write queue is infinite
        for (unsigned i = 0; need_block && (i < ifaces_.size()); i++)
        {
            const bool need_read = inout_masks.read  & (1 << i);
            if (need_read && ifaces_[i]->hasReadyRx())
            {
                need_block = false;
            }
        }

        if (need_block)
        {
            // Poll FD set setup
            ::pollfd pollfds[uavcan::MaxCanIfaces] = {};

            unsigned num_pollfds = 0;
            IfaceWrapper* pollfd_index_to_iface[uavcan::MaxCanIfaces] = { };

            for (unsigned i = 0; i < ifaces_.size(); i++)
            {
                if (!ifaces_[i]->isDown())
                {
                    pollfds[num_pollfds].fd = ifaces_[i]->getFileDescriptor();
                    pollfds[num_pollfds].events = POLLIN;
                    if (ifaces_[i]->hasReadyTx() || (inout_masks.write & (1U << i)))
                    {
                        pollfds[num_pollfds].events |= POLLOUT;
                    }
                    pollfd_index_to_iface[num_pollfds] = ifaces_[i].get();
                    num_pollfds++;
                }
            }

            // This is where we abort when the last iface goes down
            if (num_pollfds == 0)
            {
                throw AllIfacesDownException();
            }

            // Timeout conversion
            const std::int64_t timeout_usec = (blocking_deadline - clock_.getMonotonic()).toUSec();
            // poll() blocks indefinitely when timeout is a negative value.
            // spinOnce() is supposed to be a non-blocking call. It calls this
            // method with blocking_deadline some time in the past. When that
            // happens timeout_usec is negative. In other cases,
            // blocking_deadline should be some time in the future. And
            // everything should work fine.
            const int timeout_msec = std::max(timeout_usec / 1000, static_cast<int64_t>(0));

            // Blocking here
            const int res = ::poll(pollfds, num_pollfds, timeout_msec);
            if (res < 0)
            {
                return res;
            }

            // Handling poll output
            for (unsigned i = 0; i < num_pollfds; i++)
            {
                pollfd_index_to_iface[i]->updateDownStatusFromPollResult(pollfds[i]);

                const bool poll_read  = pollfds[i].revents & POLLIN;
                const bool poll_write = pollfds[i].revents & POLLOUT;
                pollfd_index_to_iface[i]->poll(poll_read, poll_write);
            }
        }

        // Writing the output masks
        inout_masks = uavcan::CanSelectMasks();
        for (unsigned i = 0; i < ifaces_.size(); i++)
        {
            if (!ifaces_[i]->isDown())
            {
                inout_masks.write |= std::uint8_t(1U << i);     // Always ready to write if not down
            }
            if (ifaces_[i]->hasReadyRx())
            {
                inout_masks.read |= std::uint8_t(1U << i);      // Readability depends only on RX buf, even if down
            }
        }

        // Return value is irrelevant as long as it's non-negative
        return ifaces_.size();
    }

    SocketCanIface* getIface(std::uint8_t iface_index) override
    {
        return (iface_index >= ifaces_.size()) ? nullptr : ifaces_[iface_index].get();
    }

    std::uint8_t getNumIfaces() const override { return ifaces_.size(); }

    /**
     * Adds one iface by name. Will fail if there are @ref MaxIfaces ifaces registered already.
     * @param iface_name E.g. "can0", "vcan1"
     * @return Negative on error, interface index on success.
     * @throws uavcan_posix::Exception.
     */
    int addIface(const std::string& iface_name)
    {
        if (ifaces_.size() >= uavcan::MaxCanIfaces)
        {
            return -1;
        }

        // Open the socket
        const int fd = SocketCanIface::openSocket(iface_name);
        if (fd < 0)
        {
            return fd;
        }

        // Construct the iface - upon successful construction the iface will take ownership of the fd.
        try
        {
            ifaces_.emplace_back(new IfaceWrapper(clock_, fd));
        }
        catch (...)
        {
            (void)::close(fd);
            throw;
        }

        UAVCAN_TRACE("SocketCAN", "New iface '%s' fd %d", iface_name.c_str(), fd);

        return ifaces_.size() - 1;
    }

    /**
     * Returns false if the specified interface is functioning, true if it became unavailable.
     */
    bool isIfaceDown(std::uint8_t iface_index) const
    {
        return ifaces_.at(iface_index)->isDown();
    }
};

} // end namespace uavcan_posix
