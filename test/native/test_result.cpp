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
    ASSERT_TRUE(!!libuavcan::Result::success);
    ASSERT_FALSE(!libuavcan::Result::success);
    ASSERT_TRUE(!libuavcan::Result::failure);
    ASSERT_FALSE(!!libuavcan::Result::failure);
}

TEST(ResultTest, ResultSuccessHelper)
{
    ASSERT_TRUE(libuavcan::success(libuavcan::Result::success));
    ASSERT_FALSE(libuavcan::success(libuavcan::Result::failure));
    ASSERT_TRUE(libuavcan::success(libuavcan::Result::success_timeout));
    ASSERT_FALSE(libuavcan::success(libuavcan::Result::bad_argument));
}

TEST(ResultTest, ResultFailureHelper)
{
    ASSERT_TRUE(libuavcan::failure(libuavcan::Result::failure));
    ASSERT_FALSE(libuavcan::failure(libuavcan::Result::success));
    ASSERT_TRUE(libuavcan::failure(libuavcan::Result::bad_argument));
    ASSERT_FALSE(libuavcan::failure(libuavcan::Result::success_nothing));
}
