/*
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Unit tests of the libcyphal::Result enum and it associated utilities.
 */

#include "libcyphal/libcyphal.hpp"
#include "lvs/lvs.hpp"

/**
 * Verify that the global ! operator works as intended.
 */
TEST(ResultTest, ResultBangOperator)
{
    ASSERT_TRUE(!!libcyphal::Result::Success);
    ASSERT_FALSE(!libcyphal::Result::Success);
    ASSERT_TRUE(!libcyphal::Result::Failure);
    ASSERT_FALSE(!!libcyphal::Result::Failure);
}

TEST(ResultTest, ResultSuccessHelper)
{
    ASSERT_TRUE(libcyphal::isSuccess(libcyphal::Result::Success));
    ASSERT_FALSE(libcyphal::isSuccess(libcyphal::Result::Failure));
    ASSERT_TRUE(libcyphal::isSuccess(libcyphal::Result::SuccessTimeout));
    ASSERT_FALSE(libcyphal::isSuccess(libcyphal::Result::BadArgument));
}

TEST(ResultTest, ResultFailureHelper)
{
    ASSERT_TRUE(libcyphal::isFailure(libcyphal::Result::Failure));
    ASSERT_FALSE(libcyphal::isFailure(libcyphal::Result::Success));
    ASSERT_TRUE(libcyphal::isFailure(libcyphal::Result::BadArgument));
    ASSERT_FALSE(libcyphal::isFailure(libcyphal::Result::SuccessNothing));
}
