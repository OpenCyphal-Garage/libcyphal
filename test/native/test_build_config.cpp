/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Unit tests of the build_config.hpp header.
 */

#include "uavcan/uavcan.hpp"
#include "gtest/gtest.h"

TEST(BuildConfigTest, Version) {
    ASSERT_EQ(2, UAVCAN_VERSION_MAJOR) << "These tests are designed for version 2 of libuavcan" << std::endl;
}
