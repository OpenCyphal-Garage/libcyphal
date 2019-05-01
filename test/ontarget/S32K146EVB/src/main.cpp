/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include <stdlib.h>
#include <errno.h>
#include <iostream>
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "S32K146.h" /* include peripheral declarations S32K146 */

extern "C"
{
    char* getcwd(char* buf, size_t size)
    {
        if (!buf || size <= 1)
        {
            errno = EINVAL;
            return nullptr;
        }
        buf[0] = '/';
        buf[1] = 0;
        return buf;
    }

    int mkdir(const char* path, mode_t mode)
    {
        (void) path;
        (void) mode;
        errno = EACCES;
        return -1;
    }

}  // extern "C"

int main()
{
    // This also initialized googletest.
    testing::InitGoogleMock();

    do
    {
        int result = RUN_ALL_TESTS();
        (void) result;
    } while (true);
    return 0;
}
