/*
 * System clock interface for posix.
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved. 
 */

#ifndef UAVCAN_POSIX_SYSTEM_CLOCK_HPP_INCLUDED
#define UAVCAN_POSIX_SYSTEM_CLOCK_HPP_INCLUDED

#include <uavcan/driver/system_clock.hpp>

namespace uavcan_posix
{

/**
 * System clock interface for posix - monotonic and UTC.
 */
class UAVCAN_EXPORT ISystemClock : public virtual uavcan::ISystemClock
{
public:
    virtual ~ISystemClock() { }

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
    virtual uavcan::UtcDuration getAdjustUtc() const = 0;

};

} // namespace uavcan_posix

#endif // UAVCAN_POSIX_SYSTEM_CLOCK_HPP_INCLUDED
