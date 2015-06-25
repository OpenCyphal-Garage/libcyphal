/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#ifndef UAVCAN_DYNAMIC_MEMORY_HPP_INCLUDED
#define UAVCAN_DYNAMIC_MEMORY_HPP_INCLUDED

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <uavcan/std.hpp>
#include <uavcan/util/templates.hpp>
#include <uavcan/util/placement_new.hpp>
#include <uavcan/build_config.hpp>

namespace uavcan
{
/**
 * This interface is used by other library components that need dynamic memory.
 */
class UAVCAN_EXPORT IPoolAllocator
{
public:
    virtual ~IPoolAllocator() { }

    virtual void* allocate(std::size_t size) = 0;
    virtual void deallocate(const void* ptr) = 0;

    virtual uint16_t getNumBlocks() const = 0;
};

/**
 * Classic implementation of a pool allocator (Meyers).
 */
template <std::size_t PoolSize, uint8_t BlockSize>
class UAVCAN_EXPORT PoolAllocator : public IPoolAllocator, Noncopyable
{
    union Node
    {
        uint8_t data[BlockSize];
        Node* next;
    };

    Node* free_list_;
    union
    {
         uint8_t bytes[PoolSize];
         long double _aligner1;
         long long _aligner2;
         Node _aligner3;
    } pool_;

    uint16_t used_;
    uint16_t max_used_;

public:
    static const uint16_t NumBlocks = PoolSize / BlockSize;

    PoolAllocator();

    virtual void* allocate(std::size_t size);
    virtual void deallocate(const void* ptr);

    virtual uint16_t getNumBlocks() const { return NumBlocks; }

    /**
     * Return the number of blocks that are currently allocated/unallocated.
     */
    uint16_t getNumUsedBlocks() const { return used_; }
    uint16_t getNumFreeBlocks() const { return static_cast<uint16_t>(NumBlocks - used_); }

    /**
     * Returns the maximum number of blocks that were ever allocated at the same time.
     */
    uint16_t getPeakNumUsedBlocks() const { return max_used_; }
};

/**
 * Limits the maximum number of blocks that can be allocated in a given allocator.
 */
class LimitedPoolAllocator : public IPoolAllocator
{
    IPoolAllocator& allocator_;
    const uint16_t max_blocks_;
    uint16_t used_blocks_;

public:
    LimitedPoolAllocator(IPoolAllocator& allocator, std::size_t max_blocks)
        : allocator_(allocator)
        , max_blocks_(static_cast<uint16_t>(min<std::size_t>(max_blocks, 0xFFFFU)))
        , used_blocks_(0)
    {
        UAVCAN_ASSERT(max_blocks_ > 0);
    }

    virtual void* allocate(std::size_t size);
    virtual void deallocate(const void* ptr);

    virtual uint16_t getNumBlocks() const;
};

// ----------------------------------------------------------------------------

/*
 * PoolAllocator<>
 */
template <std::size_t PoolSize, uint8_t BlockSize>
const uint16_t PoolAllocator<PoolSize, BlockSize>::NumBlocks;

template <std::size_t PoolSize, uint8_t BlockSize>
PoolAllocator<PoolSize, BlockSize>::PoolAllocator() :
    free_list_(reinterpret_cast<Node*>(pool_.bytes)),
    used_(0),
    max_used_(0)
{
    // The limit is imposed by the width of the pool usage tracking variables.
    StaticAssert<((PoolSize / BlockSize) <= 0xFFFFU)>::check();

    (void)std::memset(pool_.bytes, 0, PoolSize);
    for (unsigned i = 0; (i + 1) < (NumBlocks - 1 + 1); i++) // -Werror=type-limits
    {
        // coverity[dead_error_line : FALSE]
        free_list_[i].next = free_list_ + i + 1;
    }
    free_list_[NumBlocks - 1].next = NULL;
}

template <std::size_t PoolSize, uint8_t BlockSize>
void* PoolAllocator<PoolSize, BlockSize>::allocate(std::size_t size)
{
    if (free_list_ == NULL || size > BlockSize)
    {
        return NULL;
    }

    void* pmem = free_list_;
    free_list_ = free_list_->next;

    // Statistics
    UAVCAN_ASSERT(used_ < NumBlocks);
    used_++;
    if (used_ > max_used_)
    {
        max_used_ = used_;
    }

    return pmem;
}

template <std::size_t PoolSize, uint8_t BlockSize>
void PoolAllocator<PoolSize, BlockSize>::deallocate(const void* ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    Node* p = static_cast<Node*>(const_cast<void*>(ptr));
    p->next = free_list_;
    free_list_ = p;

    // Statistics
    UAVCAN_ASSERT(used_ > 0);
    used_--;
}

}

#endif // UAVCAN_DYNAMIC_MEMORY_HPP_INCLUDED
