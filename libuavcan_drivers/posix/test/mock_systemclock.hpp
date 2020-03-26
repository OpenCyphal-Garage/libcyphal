/*
 * Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <uavcan_posix/system_clock.hpp>

namespace uavcan_posix {

class SystemClockMock : public uavcan_posix::ISystemClock
{
public:
    virtual ~SystemClockMock() { }

    MOCK_CONST_METHOD0(getMonotonic, uavcan::MonotonicTime());
    MOCK_CONST_METHOD0(getUtc, uavcan::UtcTime());
    MOCK_METHOD1(adjustUtc, void(uavcan::UtcDuration));
    MOCK_CONST_METHOD0(getAdjustUtc, uavcan::UtcDuration());
};

} // end namespace uavcan_posix
