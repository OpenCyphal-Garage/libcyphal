/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Unit tests of the libuavcan::Result enum and it associated utilities.
 */

#include "libuavcan/libuavcan.hpp"
#include "lvs/lvs.hpp"

/**
 * Verify that the global ! operator works as intended.
 */
TEST(ResultTest, ResultBangOperator)
{
    ASSERT_TRUE(!!libuavcan::Result::Success);
    ASSERT_FALSE(!libuavcan::Result::Success);
    ASSERT_TRUE(!libuavcan::Result::Failure);
    ASSERT_FALSE(!!libuavcan::Result::Failure);
}

TEST(ResultTest, ResultSuccessHelper)
{
    ASSERT_TRUE(libuavcan::isSuccess(libuavcan::Result::Success));
    ASSERT_FALSE(libuavcan::isSuccess(libuavcan::Result::Failure));
    ASSERT_TRUE(libuavcan::isSuccess(libuavcan::Result::SuccessTimeout));
    ASSERT_FALSE(libuavcan::isSuccess(libuavcan::Result::BadArgument));
}

TEST(ResultTest, ResultFailureHelper)
{
    ASSERT_TRUE(libuavcan::isFailure(libuavcan::Result::Failure));
    ASSERT_FALSE(libuavcan::isFailure(libuavcan::Result::Success));
    ASSERT_TRUE(libuavcan::isFailure(libuavcan::Result::BadArgument));
    ASSERT_FALSE(libuavcan::isFailure(libuavcan::Result::SuccessNothing));
}
