/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Unit tests of the build_config.hpp header.
 */

#include "libuavcan/libuavcan.hpp"
#include "lvs/lvs.hpp"

/**
 * Test that the version numbers in the build_config header are expected.
 */
TEST(BuildConfigTest, Version) {
    ASSERT_EQ(2, LIBUAVCAN_VERSION_MAJOR) << "These tests are designed for version 2 of libuavcan" << std::endl;
    ASSERT_EQ(1, LIBUAVCAN_UAVCAN_SPECIFICATION_VERSION_MAJOR) << "These tests are designed for version 1 of the UAVCAN protocol" << std::endl;
}

/**
 * Force proper compilation of standard, regulated dsdl.
 */
TEST(BuildConfigTest, dsdl_regulated) {

}
