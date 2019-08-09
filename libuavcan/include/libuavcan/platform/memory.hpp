/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#ifndef LIBUAVCAN_PLATFORM_MEMORY_HPP_INCLUDED
#define LIBUAVCAN_PLATFORM_MEMORY_HPP_INCLUDED

#include <atomic>
#include <unordered_map>
#include "libuavcan/libuavcan.hpp"
#include "libuavcan/introspection.hpp"

namespace libuavcan
{
namespace platform
{
namespace memory
{
/**
 * Classic implementation of a pool allocator (Meyers).
 *
 * This instance is designed to be a process-wide singleton and is not designed for direct access
 * by applications or libuavcan. To use this memory pool it is recommended that one of the allocators
 * defined in this header is used.
 *
 * This implementation relies on STL atomics and thread-safe static compiler support to be thread-safe.
 * If compiling with -fno-threadsafe-statics then the application must ensure no concurrent access is
 * attempted the first time getInstance() is invoked. After this first access all subsequent access
 * is thread-safe regardless of thread-safe static support.
 *
 * @tparam  NumBlocksParam  The number of blocks to allocate in the pool. Unlike an arena allocator,
 *                          this allocator does not guarantee a continuous block of memory and each
 *                          memory block can located anywhere on the system. It is highly recommended
 *                          that all memory blocks have the same access permsissions and performance.
 * @tparam  BlockSizeParam  The size in bytes of each block. Note that all blocks will use this size
 *                          such that the amount of memory allocated for the pool will be at least
 *                          NumBlocksParam x BlockSizeParam bytes. Implementations may choose to
 *                          allocate additional memory per-block to detect buffer overrun and illegal
 *                          deallocation.
 */
template <std::size_t NumBlocksParam, std::uint8_t BlockSizeParam>
class LIBUAVCAN_EXPORT StaticMemoryPool final
{
    /**
     * A cute little trick is used by Pavel here where the data
     * is what we expose to clients but internally we use the
     * next pointer. Since we do not share the block with clients
     * (i.e. we are hands-off after allocation and the client must
     * be hands-off after deallocation) it doesn't matter that the
     * fields overlap and overwrite each other.
     */
    union Block
    {
        /**
         * The client representation of the block. Not used
         * by this class internally other than to allocate
         * storage.
         */
        std::uint8_t data[BlockSizeParam];

        /**
         * The internal representation of the block. This
         * becomes invalid after allocation and must be
         * reinitialized after deallocation.
         */
        Block* next;

        /**
         * Added to ensure this block will be aligned for any
         * scalar type on the platform and to ensure that
         * pointers to Block instances have the same alignment
         * guarantees as malloc.
         */
        std::max_align_t alignment_type;
    };

    /**
     * This is the big chunk of memory vended out in BlockSizeParam chunks via
     * the allocate method.
     */
    typename std::aligned_storage<sizeof(Block), alignof(Block)>::type storage_[NumBlocksParam];

    /**
     * The head of our free list. Note that this is a singly linked list and is unordered.
     * If we have a pointer to a block then it is deallocated. We do not track allocations currently
     * although we should since an application can insert data that is not in the pool by deallocating
     * the wrong pointer.
     */
    std::atomic<Block*> free_list_;

    StaticMemoryPool() noexcept
        : storage_()
        , free_list_()
    {
        free_list_.store(reinterpret_cast<Block*>(&storage_));
        static_assert(NumBlocksParam > 0, "Cannot create a PoolAllocator with less than 1 block.");

        Block* list = free_list_.load();
        Block* next = &list[NumBlocksParam - 1];
        next->next  = nullptr;
        for (std::size_t i = NumBlocksParam - 1; i > 0; --i)
        {
            Block* previous = &list[i - 1];
            previous->next  = next;
            next            = previous;
        }
    }

    // +------------------------------------------------------------------+
    // | RULE OF SIX
    // +------------------------------------------------------------------+
    ~StaticMemoryPool() = default;

public:
    StaticMemoryPool(const StaticMemoryPool&)  = delete;
    StaticMemoryPool(const StaticMemoryPool&&) = delete;
    StaticMemoryPool& operator=(const StaticMemoryPool&) = delete;
    StaticMemoryPool& operator=(const StaticMemoryPool&&) = delete;

private:
    // +------------------------------------------------------------------+
    // | ALLOCATORS
    // |    A list of allocators that can use this memory pool.
    // +------------------------------------------------------------------+
    template <std::size_t, std::uint8_t, typename, typename>
    friend class PoolAllocator;

    /**
     * Get a reference to the static memory pool. The first time this is called
     * the memory pool will perform internal initialization that is only thread-safe
     * if the compiler has generated the proper thread-safe static access instructions.
     */
    static StaticMemoryPool& getReference()
    {
        static StaticMemoryPool pool_;
        return pool_;
    }

    void* allocate(std::size_t size)
    {
        Block* previous = nullptr;
        if (size <= BlockSizeParam)
        {
            Block* next = nullptr;
            do
            {
                previous = free_list_.load();
                if (previous == nullptr)
                {
#if LIBUAVCAN_ENABLE_EXCEPTIONS
                    std::bad_alloc exception;
                    throw exception;
#endif
                    break;
                }
                next = previous->next;
            } while (!std::atomic_compare_exchange_weak(&free_list_, &previous, next));
        }
        else
        {
#if LIBUAVCAN_ENABLE_EXCEPTIONS
            std::bad_alloc exception;
            throw exception;
#endif
        }

        return previous;
    }

    void deallocate(void* ptr)
    {
        if (ptr != nullptr)
        {
            Block* reclaimed_block = reinterpret_cast<Block*>(ptr);
            reclaimed_block->next  = std::atomic_exchange(&free_list_, reclaimed_block);
        }
    }
};

/**
 * Adapter to provide STL and other instances access to a shared memory pool.
 *
 * @tparam  NumBlocksParam      The number of blocks to allocate in the memory pool.
 * @tparam  BlockSizeParam      The size in bytes of each block in the memory pool.
 * @tparam T                    A type alias used to pretend that the pointers returned are pointers to
 *                              this type. C++ requires typed pointers in allocators so setting this to
 *                              void will fail for some uses of this allocator with STL.
 * @tparam MemoryPoolType       Object that must support our static memory pool accessor concept. This
 *                              concept requires the following:
 *                              1. A static getReference() method that returns a reference to a memory
 *                                 allocator that shall remain value for at least the life of this instance.
 *                              2. An allocate(std::size_t size) method to allocate a single block of memory of at
 *                                 least size bytes.
 *                              3. A deallocate(void* p) method to deallocate memory returned from the allocate
 *                                 method.
 *                              4. allocate and deallocate shall be thread-safe.
 *                              5. sizeof(T) shall be >= BlockSizeParam.
 */
template <std::size_t  NumBlocksParam,
          std::uint8_t BlockSizeParam,
          typename T              = std::uint8_t,
          typename MemoryPoolType = StaticMemoryPool<NumBlocksParam, BlockSizeParam>>
class LIBUAVCAN_EXPORT PoolAllocator
{
    MemoryPoolType& pool_;

public:
    explicit PoolAllocator() noexcept
        : pool_(MemoryPoolType::getReference())
    {}

    explicit PoolAllocator(const PoolAllocator& rhs) noexcept
        : pool_(rhs.pool_)
    {}

    explicit PoolAllocator(const PoolAllocator&& rhs) noexcept
        : pool_(rhs.pool_)
    {}

    ~PoolAllocator() = default;

    template <typename T1>
    PoolAllocator(const PoolAllocator<NumBlocksParam, BlockSizeParam, T1, MemoryPoolType>&) noexcept
        : pool_(MemoryPoolType::getReference())
    {}

    static_assert(sizeof(typename std::conditional<std::is_same<void, T>::value, std::uint8_t, T>::type) <=
                      BlockSizeParam,
                  "Type alias T must fit within the specified block size!");

    // Definitions required by STL (https://en.cppreference.com/w/cpp/named_req/Allocator)
    using pointer         = T*;
    using const_pointer   = const T*;
    using value_type      = T;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;

    template <typename T1>
    struct rebind
    {
        using other = PoolAllocator<NumBlocksParam, BlockSizeParam, T1, MemoryPoolType>;
    };

    /**
     * The size in bytes of each memory block in the pool. See BlockSizeParam for additional documentation.
     */
    constexpr static const std::size_t BlockSize = BlockSizeParam;

    /**
     * The number of blocks managed by this pool. See NumBlocksParam for additional documentation.
     */
    constexpr static const std::size_t NumBlocks = NumBlocksParam;

    // +----------------------------------------------------------------------+
    // | STL ALLOCATOR CONCEPT
    // +----------------------------------------------------------------------+
    /**
     * @deprecated
     * Provided for STL support. Do not use directly. This will be removed
     * in a future version of c++.
     */
    pointer allocate(size_type n, const void* hint)
    {
        // hint is deprecated and removed in c++20. Just
        // ignore it.
        (void) hint;
        return allocate(n);
    }

    /**
     * Allocate a single block of memory that is at lease @p size bytes in length.
     *
     * @param size  The number of bytes required.
     * @return A pointer to a block of memory that is >= size bytes and <= BlockSize bytes.
     *         If size is > BlockSize then nullptr is returned.
     *         If there are no more available blocks then nullptr is returned.
     */
    pointer allocate(size_type size)
    {
        return reinterpret_cast<T*>(pool_.allocate(size));
    }

    /**
     * Return a previously allocated block to the pool.
     *
     * @param ptr   The pointer to the block returned from allocate.
     * @param size  The size of the block requested in the call to allocate.
     */
    void deallocate(pointer ptr, size_type size)
    {
        // TODO: check size and track allocated pointers in a diagnostic mode
        //       that can reveal deallocation of non-pool memory.
        (void) size;

        pool_.deallocate(ptr);
    }

    /**
     * Return the largest value that can be passed to allocate.
     */
    size_type max_size() const
    {
        return BlockSizeParam;
    }
};

/**
 * Copy bits from a byte array using arbitrary alignment to an aligned byte array.
 *
 * @param  src              The byte array to copy from.
 * @param  src_offset_bits  The offset, in bits, from the start of the src array to
 *                          start copying from.
 * @param  dst              The byte array to copy data into.
 * @param  length_bits      The total length of bits to copy. The caller must ensure
 *                          that the size of src and dst are >= this value.
 *
 * @return The number of bits copied.
 */
inline std::size_t copyBitsUnalignedToAligned(const std::uint8_t* const src,
                                              const std::size_t         src_offset_bits,
                                              std::uint8_t* const       dst,
                                              const std::size_t         length_bits)
{
    if (nullptr == src || nullptr == dst || length_bits == 0)
    {
        return 0;
    }
    std::size_t       bits_copied  = 0;
    std::size_t       offset_bits  = src_offset_bits;
    const std::size_t local_offset = src_offset_bits % 8U;
    do
    {
        std::size_t       current_byte       = offset_bits / 8U;
        const std::size_t bits_from_src_byte = 8U - local_offset;
        bits_copied += std::min(length_bits, bits_from_src_byte);
        dst[current_byte] &= static_cast<std::uint8_t>(0xFF << bits_from_src_byte);
        dst[current_byte] |= static_cast<std::uint8_t>(src[current_byte] >> local_offset);
        offset_bits += 8U;
        if (offset_bits < length_bits)
        {
            current_byte      = offset_bits / 8U;
            dst[current_byte] = static_cast<std::uint8_t>(src[current_byte] << bits_from_src_byte);
            bits_copied += local_offset;
        }
        else
        {
            // we don't need to reevaluate the while condition.
            break;
        }
    } while (true);
    return bits_copied;
}

/**
 * Copy aligned bits from a byte array to another byte array using arbitrary alignment.
 *
 * @param  src              The byte array to copy from.
 * @param  dst              The byte array to copy data into.
 * @param  dst_offset_bits  The offset, in bits, from the start of the dst array to
 *                          start writing to.
 * @param  length_bits      The total length of bits to copy. The caller must ensure
 *                          that the size of src and dst are >= this value.
 *
 * @return The number of bits copied.
 */
inline std::size_t copyBitsAlignedToUnaligned(const std::uint8_t* const src,
                                              std::uint8_t* const       dst,
                                              const std::size_t         dst_offset_bits,
                                              const std::size_t         length_bits)
{
    if (nullptr == src || nullptr == dst || length_bits == 0)
    {
        return 0;
    }
    std::size_t       bits_copied  = 0;
    std::size_t       offset_bits  = dst_offset_bits;
    const std::size_t local_offset = dst_offset_bits % 8U;
    do
    {
        std::size_t       current_byte       = offset_bits / 8U;
        const std::size_t bits_from_src_byte = 8U - local_offset;
        dst[current_byte] &= static_cast<std::uint8_t>(0xFF >> bits_from_src_byte);
        dst[current_byte] |= static_cast<std::uint8_t>(src[current_byte] << local_offset);
        offset_bits += 8U;
        bits_copied += std::min(length_bits, bits_from_src_byte);
        if (offset_bits < length_bits)
        {
            dst[current_byte] |= static_cast<std::uint8_t>(src[offset_bits / 8U] >> bits_from_src_byte);
            bits_copied += local_offset;
        }
        else
        {
            // we don't need to reevaluate the while condition.
            break;
        }
    } while (true);

    return bits_copied;
}

}  // namespace memory
}  // namespace platform
}  // namespace libuavcan

#endif  // LIBUAVCAN_PLATFORM_MEMORY_HPP_INCLUDED
