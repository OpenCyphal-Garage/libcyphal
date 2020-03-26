/*
 * Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#include <gtest/gtest.h>
#include <uavcan_posix/socketcan.hpp>

#include "mock_systemclock.hpp"

namespace uavcan_posix {

/**
 * Tests the SocketCanIface object.
 */
class SocketCanIfaceTest : public ::testing::Test
{

protected:
    uavcan_posix::can_frame friendly_makeSocketCanFrame(const uavcan::CanFrame& uavcan_frame) 
    {
        return uavcan_posix::SocketCanIface::makeSocketCanFrame(uavcan_frame);
    }

    uavcan::CanFrame friendly_makeUavcanFrame(uavcan_posix::can_frame& sockcan_frame)
    {
        return uavcan_posix::SocketCanIface::makeUavcanFrame(sockcan_frame);
    }
};

/**
 * Ensure makeSocketCanFrame copies values from the provided input frame.
 */
TEST_F(SocketCanIfaceTest, makeSocketCanFrame) 
{
    uint8_t fakeData[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    uavcan::CanFrame input_frame(9, fakeData, 8);

    uavcan_posix::can_frame result = friendly_makeSocketCanFrame(input_frame);
    EXPECT_EQ(input_frame.id, result.can_id);
    EXPECT_EQ(8, result.len);
    EXPECT_EQ(0, ::memcmp(fakeData, result.data, sizeof(fakeData)));
}

/**
 * Ensure makeUavcanFrame copies values from the provided input frame.
 */
TEST_F(SocketCanIfaceTest, makeUavcanFrame) 
{
    uint8_t fakeData[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    uavcan_posix::can_frame input_frame;
    input_frame.can_id = 2;
    input_frame.len = 8;
    std::copy(fakeData, fakeData + sizeof(fakeData), input_frame.data);

    uavcan::CanFrame result = friendly_makeUavcanFrame(input_frame);
    EXPECT_EQ(result.id, input_frame.can_id);
    EXPECT_EQ(result.dlc, 8);
    EXPECT_EQ(0, ::memcmp(result.data, fakeData, sizeof(fakeData)));
}

/**
 * Ensure that creation of the SocketCanIface in debug builds
 * does assert if the provided fd < 0
 */
TEST_F(SocketCanIfaceTest, createIfaceWithNegativeFd)
{
    SystemClockMock mockclock;
    ASSERT_DEATH(SocketCanIface test_subject(mockclock, -1),"");
}

/**
 * Ensure that creation of the SocketCanIface in debug builds
 * does assert if the provided fd < 0
 */
TEST_F(SocketCanIfaceTest, createIfaceWith0Fd)
{
    SystemClockMock mockclock;
    SocketCanIface test_subject(mockclock, 0);
    (void)test_subject;
}


} // end namespace uavcan_posix
