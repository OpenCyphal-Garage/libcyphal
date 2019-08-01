/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Unit tests of the pool allocator in the platform layer used with STL.
 */
#include "lvs/lvs.hpp"

#include "lvs/platform/memory.hpp"

namespace lvs
{
namespace platform
{
namespace memory
{
typedef ::testing::Types<
    libuavcan::platform::memory::PoolAllocator<PoolAllocatorUnorderedMapTestRequirements::MinimumBlockCount,
                                               PoolAllocatorUnorderedMapTestRequirements::MinimumBlockSize,
                                               PoolAllocatorUnorderedMapTestRequirements::AllocatorPtrType>>
    MyTypes;

// The trailing comma is required. See https://github.com/google/googletest/issues/1419
INSTANTIATE_TYPED_TEST_SUITE_P(Generic, PoolAllocatorUnorderedMapTest, MyTypes, );

}  // namespace memory
}  // namespace platform
}  // namespace lvs
