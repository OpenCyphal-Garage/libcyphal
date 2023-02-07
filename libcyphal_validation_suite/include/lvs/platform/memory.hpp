/*
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/** @file
 * Implement this test to verify that your platform provides the required
 * memory management support to build and run libcyphal.
 *
 * <h3>Example:</H3>
 * @code
 * #include "lvs/lvs.hpp"
 * #include "lvs/platform/memory.hpp"
 *
 * namespace lvs
 * {
 * namespace platform
 * {
 * namespace memory
 * {
 *
 * typedef ::testing::Types<libcyphal::platform::memory::PoolAllocator<1, 1, char>,
 *                          libcyphal::platform::memory::PoolAllocator<72, std::numeric_limits<std::uint8_t,
 * int>::max()>> MyPoolAllocatorTypes;
 *
 * // The trailing comma is required. See https://github.com/google/googletest/issues/1419
 * INSTANTIATE_TYPED_TEST_SUITE_P(MyTest, PoolAllocatorTest, MyPoolAllocatorTypes, );
 *
 * }  // namespace memory
 * }  // namespace platform
 * }  // namespace lvs
 * @endcode
 */
#ifndef LIBCYPHAL_LVS_PLATFORM_MEMORY_HPP_INCLUDED
#define LIBCYPHAL_LVS_PLATFORM_MEMORY_HPP_INCLUDED

#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

#include "gtest/gtest.h"

#include "lvs/lvs.hpp"
#include "libcyphal/platform/memory.hpp"

namespace lvs
{
namespace platform
{
namespace memory
{
// +--------------------------------------------------------------------------+
// | POOL ALLOCATOR TEST
// +--------------------------------------------------------------------------+
/**
 * Test fixture for testing the provided PoolAllocator on your platform using
 * various parameters.
 * See https://github.com/google/googletest/blob/master/googletest/docs/advanced.md for
 * more information about typed gtests.
 *
 * @tparam T    The allocator type under test.
 */
template <typename T>
class PoolAllocatorTest : public ::testing::Test
{};

TYPED_TEST_SUITE_P(PoolAllocatorTest);

// +--------------------------------------------------------------------------+
// | TEST CASES
// +--------------------------------------------------------------------------+

/**
 * Cover allocations that exceed the current block size.
 */
TYPED_TEST_P(PoolAllocatorTest, AllocTooLargeBlock)
{
    TypeParam allocator;

    if (TypeParam::BlockSize < std::numeric_limits<decltype(TypeParam::BlockSize)>::max())
    {
        LVS_ASSERT_THROW_IF_EXCEPTIONS_OR_VALUE(allocator.allocate(TypeParam::BlockSize + 1), std::bad_alloc, nullptr);
    }
}

/**
 * In C/C++ free/delete/deallocate is always null safe.
 */
TYPED_TEST_P(PoolAllocatorTest, DeallocateNull)
{
    TypeParam allocator;

    allocator.deallocate(nullptr, TypeParam::BlockSize);
}

/**
 * Ensure that your allocator can allocate and free a small chunk of memory.
 */
TYPED_TEST_P(PoolAllocatorTest, AllocDealloc)
{
    TypeParam allocator;

    typename TypeParam::pointer p = allocator.allocate(1);
    ASSERT_NE(nullptr, p);
    allocator.deallocate(p, 1);
}

/**
 * Allocate everything until the pool memory is exhausted then deallocate
 * everything and verify that memory can be reclaimed.
 */
TYPED_TEST_P(PoolAllocatorTest, AllocTillOutOfMemoryThenDealloc)
{
    TypeParam allocator;

    std::array<typename TypeParam::pointer, TypeParam::NumBlocks> allocations;
    for (std::size_t i = 0; i < allocations.size(); ++i)
    {
        allocations[i] = allocator.allocate(TypeParam::BlockSize);
        ASSERT_NE(nullptr, allocations[i]);
    }

    LVS_ASSERT_THROW_IF_EXCEPTIONS_OR_VALUE(allocator.allocate(TypeParam::BlockSize), std::bad_alloc, nullptr);

    for (std::size_t i = 0; i < allocations.size(); ++i)
    {
        allocator.deallocate(allocations[i], TypeParam::BlockSize);
    }

    for (std::size_t i = 0; i < allocations.size(); ++i)
    {
        allocations[i] = allocator.allocate(TypeParam::BlockSize);
        ASSERT_NE(nullptr, allocations[i]);
    }

    LVS_ASSERT_THROW_IF_EXCEPTIONS_OR_VALUE(allocator.allocate(TypeParam::BlockSize), std::bad_alloc, nullptr);

    for (std::size_t i = 0; i < allocations.size(); ++i)
    {
        allocator.deallocate(allocations[i], TypeParam::BlockSize);
    }
}

/**
 * Allocate and deallocate more than NumBlocks of memory to ensure
 * there are no leaks.
 */
TYPED_TEST_P(PoolAllocatorTest, AllocThenDeallocContinuously)
{
    TypeParam allocator;

    for (std::size_t i = 0; i < TypeParam::NumBlocks * 2; ++i)
    {
        typename TypeParam::pointer p = allocator.allocate(TypeParam::BlockSize);
        ASSERT_NE(nullptr, p);
        allocator.deallocate(p, TypeParam::BlockSize);
    }

    typename TypeParam::pointer p = allocator.allocate(TypeParam::BlockSize);
    ASSERT_NE(nullptr, p);
    allocator.deallocate(p, TypeParam::BlockSize);
}

REGISTER_TYPED_TEST_SUITE_P(PoolAllocatorTest,  //
                            AllocTooLargeBlock,
                            DeallocateNull,
                            AllocDealloc,
                            AllocTillOutOfMemoryThenDealloc,
                            AllocThenDeallocContinuously);

// +--------------------------------------------------------------------------+
// | POOL ALLOCATOR TEST FOR STL VECTOR
// +--------------------------------------------------------------------------+
namespace PoolAllocatorVectorTestRequirements
{
/**
 * The required T type for TypeParam.
 */
using AllocatorPtrType = char;

/**
 * Required block size for the TypeParam to be valid for these tests.
 */
constexpr static const std::size_t MinimumBlockCount = 2;

/**
 * Required block size for the TypeParam to be valid for these tests.
 */
constexpr static const std::size_t MinimumBlockSize = 10;
}  // namespace PoolAllocatorVectorTestRequirements

/**
 * Verify the pool allocator is suitable for use with std::vector.
 * Of course actually doing this with a vector is incredibly dangerous since
 * vector will continuously allocate larger blocks which will quickly cause
 * the pool allocator to refuse to allocate new memory and then then the
 * vector will go kaboom!
 * See https://github.com/google/googletest/blob/master/googletest/docs/advanced.md for
 * more information about typed gtests.
 *
 * @tparam T    The allocator type under test.
 */
template <typename T>
class PoolAllocatorVectorTest : public ::testing::Test
{};

TYPED_TEST_SUITE_P(PoolAllocatorVectorTest);

// +--------------------------------------------------------------------------+
// | TEST CASES
// +--------------------------------------------------------------------------+

/**
 * Basic multi-push test.
 */
TYPED_TEST_P(PoolAllocatorVectorTest, TestVector)
{
    std::vector<typename TypeParam::value_type, TypeParam> test_vector;
    test_vector.push_back('h');
    test_vector.push_back('e');
    test_vector.push_back('l');
    test_vector.push_back('l');
    test_vector.push_back('o');
    test_vector.push_back('\0');
    ASSERT_EQ('h', test_vector[0]);
    ASSERT_STREQ("hello", reinterpret_cast<const char*>(test_vector.data()));
}

/**
 * Grow and shrink capacity test.
 */
TYPED_TEST_P(PoolAllocatorVectorTest, TestVectorCapacity)
{
    std::vector<typename TypeParam::value_type, TypeParam> test_vector;
    test_vector.reserve(PoolAllocatorVectorTestRequirements::MinimumBlockSize);
    ASSERT_EQ(PoolAllocatorVectorTestRequirements::MinimumBlockSize, test_vector.capacity());
    test_vector.shrink_to_fit();
    ASSERT_LE(test_vector.capacity(), PoolAllocatorVectorTestRequirements::MinimumBlockSize);
}

/**
 * Test emplace_back support.
 */
TYPED_TEST_P(PoolAllocatorVectorTest, TestEmplace)
{
    std::vector<typename TypeParam::value_type, TypeParam> test_vector;
    test_vector.reserve(PoolAllocatorVectorTestRequirements::MinimumBlockSize);
    test_vector.emplace_back('1');
    ASSERT_EQ('1', test_vector[0]);
}

REGISTER_TYPED_TEST_SUITE_P(PoolAllocatorVectorTest,  //
                            TestVector,
                            TestVectorCapacity,
                            TestEmplace);

// +--------------------------------------------------------------------------+
// | POOL ALLOCATOR TEST FOR STL HASH MAP
// +--------------------------------------------------------------------------+
namespace PoolAllocatorUnorderedMapTestRequirements
{
/**
 * Required block size for the TypeParam to be valid for these tests.
 */
constexpr static const std::size_t MinimumBlockCount = 16;

/**
 * The required T type for TypeParam.
 */
using AllocatorPtrType = std::pair<const int, int>;

/**
 * Required block size for the TypeParam to be valid for these tests.
 */
constexpr static const std::size_t MinimumBlockSize = sizeof(AllocatorPtrType) + 64;
}  // namespace PoolAllocatorUnorderedMapTestRequirements

/**
 * Verify that the pool allocator can be used with std::unordered_map.
 * See https://github.com/google/googletest/blob/master/googletest/docs/advanced.md for
 * more information about typed gtests.
 *
 * @tparam T    The allocator type under test.
 */
template <typename T>
class PoolAllocatorUnorderedMapTest : public ::testing::Test
{};

TYPED_TEST_SUITE_P(PoolAllocatorUnorderedMapTest);

// +--------------------------------------------------------------------------+
// | TEST CASES
// +--------------------------------------------------------------------------+

TYPED_TEST_P(PoolAllocatorUnorderedMapTest, TestUnorderedMap)
{
    std::unordered_map<int, int, std::hash<int>, std::equal_to<int>, TypeParam> test_map;
    test_map[1] = 2;
    test_map[2] = 4;
    test_map[3] = 8;
    ASSERT_EQ(2, test_map[1]);
    ASSERT_EQ(4, test_map[2]);
    ASSERT_EQ(8, test_map[3]);
}

REGISTER_TYPED_TEST_SUITE_P(PoolAllocatorUnorderedMapTest,  //
                            TestUnorderedMap);
}  // namespace memory
}  // namespace platform
}  // end namespace lvs

#endif  // LIBCYPHAL_LVS_PLATFORM_MEMORY_HPP_INCLUDED
