/*
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Unit tests of the build_config.hpp header.
 */

#include "libcyphal/libcyphal.hpp"
#include "lvs/lvs.hpp"

/**
 * Test that the version numbers in the build_config header are expected.
 */
TEST(BuildConfigTest, Version) {
    ASSERT_EQ(2, LIBCYPHAL_VERSION_MAJOR) << "These tests are designed for version 2 of libcyphal" << std::endl;
    ASSERT_EQ(1, LIBCYPHAL_CYPHAL_SPECIFICATION_VERSION_MAJOR) << "These tests are designed for version 1 of the Cyphal protocol" << std::endl;
}

/**
 * Force proper compilation of standard, regulated dsdl.
 */
TEST(BuildConfigTest, dsdl_regulated) {

}
