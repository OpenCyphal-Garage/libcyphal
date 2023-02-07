/*
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/** @file
 * Include this test in a google test application to verify unaligned bit copy
 * for your platform.
 */
#ifndef LIBCYPHAL_LVS_PLATFORM_MEMORY_COPY_HPP_INCLUDED
#define LIBCYPHAL_LVS_PLATFORM_MEMORY_COPY_HPP_INCLUDED

#include "lvs/lvs.hpp"
#include "libcyphal/platform/memory.hpp"

namespace lvs
{
namespace platform
{
namespace memory
{
/**
 * Cover all mundane cases where inputs are out of range or invalid.
 */
TEST(CopyBitsTest, InputsValidation)
{
    std::uint8_t* dummy_null = nullptr;
    // Just allocate a bunch of memory on the stack to keep the tests from segfaulting.
    // For the input validation test we don't care about the contents of this memory.
    std::uint8_t          dummy[]           = {0, 0, 0, 0};
    constexpr std::size_t dummy_length_bits = std::extent<decltype(dummy)>::value * 8;
    static_assert(dummy_length_bits > 16, "Test expects more than 2 bytes in the dummy dataset.");

    // null pointers
    ASSERT_EQ(0U, libcyphal::platform::memory::copyBitsUnalignedToAligned(dummy_null, 0, dummy, dummy_length_bits));
    ASSERT_EQ(0U, libcyphal::platform::memory::copyBitsAlignedToUnaligned(dummy_null, dummy, 0, dummy_length_bits));
    ASSERT_EQ(0U, libcyphal::platform::memory::copyBitsUnalignedToAligned(dummy, 0, dummy_null, dummy_length_bits));
    ASSERT_EQ(0U, libcyphal::platform::memory::copyBitsAlignedToUnaligned(dummy, dummy_null, 0, dummy_length_bits));

    // zero length arrays.
    ASSERT_EQ(0U, libcyphal::platform::memory::copyBitsUnalignedToAligned(dummy, 0, dummy, 0));
    ASSERT_EQ(0U, libcyphal::platform::memory::copyBitsAlignedToUnaligned(dummy, dummy, 0, 0));
}

// +--------------------------------------------------------------------------+
// | TEST CASES :: ONE BYTE
// +--------------------------------------------------------------------------+
/**
 * Copy from one byte aligned into another aligned.
 */
TEST(CopyBitsTest, OneByteAlignedIntoOneAligned)
{
    const std::uint8_t    src[]        = {0x55};
    std::uint8_t          dst[]        = {0xFF};
    constexpr std::size_t bits_to_copy = sizeof(std::uint8_t) * 8;
    ASSERT_EQ(bits_to_copy, libcyphal::platform::memory::copyBitsUnalignedToAligned(src, 0, dst, bits_to_copy));
    ASSERT_EQ(src[0], dst[0]);
    ASSERT_EQ(bits_to_copy, libcyphal::platform::memory::copyBitsAlignedToUnaligned(src, dst, 0, bits_to_copy));
    ASSERT_EQ(src[0], dst[0]);
}

/**
 * Copy from one byte aligned into another unaligned.
 *
 * @code
 * dst (iit)    = 11111111
 * src          = 01010101
 * dst (result) = 10101011
 *
 * dst (iit)    = 00000000
 * src          = 01010101
 * dst (result) = 10101010
 * @endcode
 */
TEST(CopyBitsTest, OneByteAlignedIntoOneUnaligned)
{
    const std::uint8_t    src[]                 = {0x55};
    std::uint8_t          dst[]                 = {0xFF};
    constexpr std::size_t bits_to_copy          = sizeof(std::uint8_t) * 8;
    constexpr std::size_t dst_offset            = 1;
    std::size_t           expected_bits_written = bits_to_copy - dst_offset;
    ASSERT_EQ(expected_bits_written,
              libcyphal::platform::memory::copyBitsAlignedToUnaligned(src, dst, dst_offset, bits_to_copy));
    ASSERT_EQ(0xAB, dst[0]);

    dst[0] = 0x00;
    ASSERT_EQ(expected_bits_written,
              libcyphal::platform::memory::copyBitsAlignedToUnaligned(src, dst, dst_offset, bits_to_copy));
    ASSERT_EQ(0xAA, dst[0]);
}

/**
 * Copy from one byte unaligned into another aligned.
 *
 * @code
 * dst (init)   = 11111111
 * src          = 01010101
 * dst (result) = 10101010
 *
 * dst (init)   = 00000000
 * src          = 01010101
 * dst (result) = 10101010
 * @endcode
 */
TEST(CopyBitsTest, OneByteUnalignedIntoOneAligned)
{
    const std::uint8_t    src[]        = {0x55};
    std::uint8_t          dst[]        = {0xFF};
    constexpr std::size_t src_offset   = 1;
    constexpr std::size_t bits_to_copy = sizeof(std::uint8_t) * 8;
    ASSERT_EQ(bits_to_copy - src_offset,
              libcyphal::platform::memory::copyBitsUnalignedToAligned(src, src_offset, dst, bits_to_copy - src_offset));
    ASSERT_EQ(0xAA, dst[0]);
    dst[0] = 0x00;
    ASSERT_EQ(bits_to_copy - src_offset,
              libcyphal::platform::memory::copyBitsUnalignedToAligned(src, src_offset, dst, bits_to_copy - src_offset));
    ASSERT_EQ(0x2A, dst[0]);
}

/**
 * Copy just two bits from an unaligned source.
 *
 * @code
 * dst (init)   = 10000000
 * src          = 11111111
 * dst (result) = 10000011
 *
 * @endcode
 */
TEST(CopyBitsTest, TwoBitsUnalignedIntoAligned)
{
    const std::uint8_t    src[]      = {0xFF};
    std::uint8_t          dst[]      = {0x80};
    constexpr std::size_t src_offset = 6;
    ASSERT_EQ(2U, libcyphal::platform::memory::copyBitsUnalignedToAligned(src, src_offset, dst, 2));
    ASSERT_EQ(0x83, dst[0]);
}

/**
 * Copy just two bits to an unaligned destination.
 *
 * @code
 * dst (init)   = 00000001
 * src          = 11111111
 * dst (result) = 11000001
 *
 * @endcode
 */
TEST(CopyBitsTest, TwoBitsAlignedIntoUnaligned)
{
    const std::uint8_t    src[]      = {0xFF};
    std::uint8_t          dst[]      = {0x1};
    constexpr std::size_t dst_offset = 6;
    ASSERT_EQ(2U, libcyphal::platform::memory::copyBitsAlignedToUnaligned(src, dst, dst_offset, 2));
    ASSERT_EQ(0xC1, dst[0]);
}

// +--------------------------------------------------------------------------+
// | TEST CASES :: MULTI BYTE
// +--------------------------------------------------------------------------+

/**
 * Copy from two integers aligned into two aligned.
 */
TEST(CopyBitsTest, TwoByteAlignedIntoTwoAligned)
{
    std::uint8_t dst0[]     = {0xFF, 0xFF};
    std::uint8_t dst1[]     = {0xFF, 0xFF};
    std::uint8_t src[]      = {0x55, 0x55};
    std::uint8_t expected[] = {0x55, 0x55};

    const std::size_t length_bits = sizeof(std::uint8_t) * 16;
    ASSERT_EQ(length_bits, libcyphal::platform::memory::copyBitsUnalignedToAligned(src, 0, dst0, length_bits));
    ASSERT_THAT(dst0, ::testing::ElementsAreArray(expected));
    ASSERT_EQ(length_bits, libcyphal::platform::memory::copyBitsAlignedToUnaligned(src, dst1, 0, length_bits));
    ASSERT_THAT(dst1, ::testing::ElementsAreArray(expected));
}

/**
 * Copy from two integers unaligned into two aligned.
 * @code
 * dst (init)   = 11111111 11111111
 * src          = 01010101 01010101
 * dst (result) = 10101010 10101010
 *
 * dst (init)   = 00000000 00000000
 * src          = 01010101 01010101
 * dst (result) = 00101010 10101010
 * @endcode
 */
TEST(CopyBitsTest, TwoByteUnalignedIntoTwoAligned)
{
    std::uint8_t dst[]      = {0xFF, 0xFF};
    std::uint8_t src[]      = {0x55, 0x55};
    std::uint8_t expected[] = {0xAA, 0xAA};

    const std::size_t length_bits = sizeof(std::uint8_t) * 16;
    ASSERT_EQ(length_bits - 1, libcyphal::platform::memory::copyBitsUnalignedToAligned(src, 1, dst, length_bits));
    ASSERT_THAT(dst, ::testing::ElementsAreArray(expected));

    dst[0]      = 0;
    dst[1]      = 0;
    expected[0] = 0x2A;
    expected[1] = 0xAA;
    ASSERT_EQ(length_bits - 1, libcyphal::platform::memory::copyBitsUnalignedToAligned(src, 1, dst, length_bits));
    ASSERT_THAT(dst, ::testing::ElementsAreArray(expected));
}

/**
 * Copy from two integers aligned into two unaligned.
 * dst (init)   = 11111111 11111111
 * src          = 10101010 10101010
 * dst (result) = 01010101 01010101
 *
 * dst (init)   = 00000000 00000000
 * src          = 10101010 10101010
 * dst (result) = 01010101 01010100
 */
TEST(CopyBitsTest, TwoByteAlignedIntoTwoUnaligned)
{
    std::uint8_t dst[]      = {0xFF, 0xFF};
    std::uint8_t src[]      = {0xAA, 0xAA};
    std::uint8_t expected[] = {0x55, 0x55};

    const std::size_t length_bits = sizeof(std::uint8_t) * 16;
    ASSERT_EQ(length_bits - 1, libcyphal::platform::memory::copyBitsAlignedToUnaligned(src, dst, 1, length_bits));
    ASSERT_THAT(dst, ::testing::ElementsAreArray(expected));

    dst[0]      = 0;
    dst[1]      = 0;
    expected[0] = 0x55;
    expected[1] = 0x54;

    ASSERT_EQ(length_bits - 1, libcyphal::platform::memory::copyBitsAlignedToUnaligned(src, dst, 1, length_bits));
    ASSERT_THAT(dst, ::testing::ElementsAreArray(expected));
}

/**
 * Sanity check that I'm not confused. Bits can be hard to reason about
 * but words either make sense or don't.
 */
TEST(CopyBitsTest, StringUnalignedIntoStringAligned)
{
    std::uint8_t          dst[]      = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::uint8_t          src[]      = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const std::uint8_t    expected[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x00};
    constexpr std::size_t src_length = std::extent<decltype(src)>::value;
    static_assert(src_length == std::extent<decltype(expected)>::value,
                  "expected string is shorter then input to copy.");

    for (std::size_t i = 0; i < src_length; ++i)
    {
        src[i] = static_cast<std::uint8_t>(expected[i] << 1);
    }
    const std::size_t length_bits = src_length * 8;
    ASSERT_EQ(length_bits - 1, libcyphal::platform::memory::copyBitsUnalignedToAligned(src, 1, dst, length_bits));
    ASSERT_THAT(dst, ::testing::ElementsAreArray(expected));
}

/**
 * Unaligned to aligned where the offset is greater than one byte.
 * dst (init)   = 11111111 11111111
 * src          = 01010101 01010101
 * dst (result) = 11111111 10101010
 *
 * dst (init)   = 00000000 00000000
 * src          = 01010101 01010101
 * dst (result) = 00000000 00101010
 *
 */
TEST(CopyBitsTest, SrcOffsetIsGreaterThanEight)
{
    std::uint8_t dst[]      = {0xFF, 0xFF};
    std::uint8_t src[]      = {0x55, 0x55};
    std::uint8_t expected[] = {0xFF, 0xAA};

    constexpr std::size_t src_length      = std::extent<decltype(src)>::value;
    const std::size_t     length_bits     = src_length * 8;
    const std::size_t     src_offset_bits = 9U;
    ASSERT_EQ(length_bits - src_offset_bits,
              libcyphal::platform::memory::copyBitsUnalignedToAligned(src, src_offset_bits, dst, length_bits));
    ASSERT_THAT(dst, ::testing::ElementsAreArray(expected));

    dst[0]      = 0;
    dst[1]      = 0;
    expected[0] = 0;
    expected[1] = 0x2A;
    ASSERT_EQ(length_bits - src_offset_bits,
              libcyphal::platform::memory::copyBitsUnalignedToAligned(src, src_offset_bits, dst, length_bits));
    ASSERT_THAT(dst, ::testing::ElementsAreArray(expected));
}

/**
 * Aligned to unaligned where the offset is greater than one byte.
 * dst (init)   = 11111111 11111111
 * src          = 01010101 01010101
 * dst (result) = 11111111 10101011
 *
 * dst (init)   = 00000000 00000000
 * src          = 01010101 01010101
 * dst (result) = 00000000 10101010
 *
 */
TEST(CopyBitsTest, DstOffsetIsGreaterThanEight)
{
    std::uint8_t dst[]      = {0xFF, 0xFF};
    std::uint8_t src[]      = {0x55, 0x55};
    std::uint8_t expected[] = {0xFF, 0xAB};

    constexpr std::size_t src_length      = std::extent<decltype(src)>::value;
    const std::size_t     length_bits     = src_length * 8;
    const std::size_t     dst_offset_bits = 9U;
    ASSERT_EQ(length_bits - dst_offset_bits,
              libcyphal::platform::memory::copyBitsAlignedToUnaligned(src, dst, dst_offset_bits, length_bits));
    ASSERT_THAT(dst, ::testing::ElementsAreArray(expected));

    dst[0]      = 0;
    dst[1]      = 0;
    expected[0] = 0;
    expected[1] = 0xAA;
    ASSERT_EQ(length_bits - dst_offset_bits,
              libcyphal::platform::memory::copyBitsAlignedToUnaligned(src, dst, dst_offset_bits, length_bits));
    ASSERT_THAT(dst, ::testing::ElementsAreArray(expected));
}

}  // namespace memory
}  // namespace platform
}  // end namespace lvs

#endif  // LIBCYPHAL_LVS_PLATFORM_MEMORY_COPY_HPP_INCLUDED
