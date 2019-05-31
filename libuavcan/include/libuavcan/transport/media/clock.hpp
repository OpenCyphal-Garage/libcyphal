/*
 * System clock driver interface.
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/** @file */

#ifndef LIBUAVCAN_TRANSPORT_MEDIA_CLOCK_HPP_INCLUDED
#define LIBUAVCAN_TRANSPORT_MEDIA_CLOCK_HPP_INCLUDED

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/time.hpp"

namespace libuavcan
{
namespace transport
{
namespace media
{
/**
 * Platform interface for a system's clock.
 */
struct LIBUAVCAN_EXPORT IClock
{
    virtual ~IClock() = default;

    /**
     * Monotontic system clock.
     *
     * This clock shall never jump or change rate; the base time is irrelevant.
     * This clock is mandatory and must remain functional at all times.
     *
     * On POSIX systems use clock_gettime() with CLOCK_MONOTONIC.
     */
    virtual MonotonicTime getMonotonic() const = 0;

    /**
     * Global network clock.
     * It doesn't have to be UTC, the name is a bit misleading - actual time base doesn't matter.
     *
     * This clock can be synchronized with other nodes on the bus, hence it can jump and/or change
     * rate occasionally.
     * This clock is optional; if it is not supported, return zero. Also return zero if the UTC time
     * is not available yet (e.g. the device has just started up with no battery clock).
     *
     * For POSIX refer to clock_gettime(), gettimeofday().
     */
    virtual UtcTime getUtc() const = 0;

    /**
     * Adjust the network-synchronized clock.
     * Refer to @ref getUtc() for details.
     *
     * For POSIX refer to adjtime(), settimeofday().
     *
     * @param [in] adjustment Amount of time to add to the clock value.
     */
    virtual void adjustUtc(UtcDuration adjustment) = 0;
};

}  // namespace media
}  // namespace transport
}  // namespace libuavcan

#endif  // LIBUAVCAN_TRANSPORT_MEDIA_CLOCK_HPP_INCLUDED
