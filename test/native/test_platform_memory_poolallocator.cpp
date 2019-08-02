/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Unit tests of the pool allocator in the platform layer.
 */
#include "lvs/lvs.hpp"

#include "lvs/platform/memory.hpp"

namespace lvs
{
namespace platform
{
namespace memory
{
typedef ::testing::Types<libuavcan::platform::memory::PoolAllocator<1, 1>,
                         libuavcan::platform::memory::PoolAllocator<1, 8, std::uint64_t>,
                         libuavcan::platform::memory::PoolAllocator<8, std::numeric_limits<std::uint8_t>::max(), char>>
    MyTypes;

// The trailing comma is required. See https://github.com/google/googletest/issues/1419
INSTANTIATE_TYPED_TEST_SUITE_P(Generic, PoolAllocatorTest, MyTypes, );

}  // namespace memory
}  // namespace platform
}  // namespace lvs
