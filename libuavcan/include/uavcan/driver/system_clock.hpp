/*
 * System clock driver interface.
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#ifndef UAVCAN_DRIVER_SYSTEM_CLOCK_HPP_INCLUDED
#define UAVCAN_DRIVER_SYSTEM_CLOCK_HPP_INCLUDED

#include <uavcan/std.hpp>
#include <uavcan/build_config.hpp>
#include <uavcan/time.hpp>

namespace uavcan
{

/**
 * System clock interface - monotonic and UTC.
 */
class UAVCAN_EXPORT ISystemClock
{
public:
    virtual ~ISystemClock() { }

    /**
     * Monototic system clock.
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

    /**
     * Get a value that should be added to any utc timestamp obtained from the system. This same value 
     * will be applied internally to values returned from getUtc().
     *
     * Note that this value is not necessarily the value set via adjustUtc(UtcDuration) but may be 
     * related for some implementations. For example, where a system clock implements adjustUtc(UtcDuration)
     * by adjusting the actual system time this method will always return 0 since time obtained from this 
     * system no longer needs to be adjusted. The opposite is true where an implementation cannot adjust 
     * the system time and so must always manually apply this adjustment to any value retrived from
     * the system.
     *
     * @return A duration to adjust (i.e. add-to) any UTC value obtained from the system.
     *
     */
    virtual UtcDuration getAdjustUtc() const = 0;
};

}

#endif // UAVCAN_DRIVER_SYSTEM_CLOCK_HPP_INCLUDED
