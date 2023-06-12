/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Allocator/Free functions for udpard

#ifndef POSIX_LIBCYPHAL_TYPES_UDPARD_HEAP_HPP_INCLUDED
#define POSIX_LIBCYPHAL_TYPES_UDPARD_HEAP_HPP_INCLUDED

#include <cstddef>
#include <udpard.h>

#include "cetl/pf17/memory_resource.hpp"

/// @brief Used to pass in allocation function to udpard
/// @param[in] udpard Instance of udpard
/// @param[in] amount Amount of memory to allocate
/// @return Pointer to allocated fragment
inline void* udpardMemAllocate(UdpardInstance* const udpard, const std::size_t amount)
{
    cetl::pf17::pmr::memory_resource* res = static_cast<cetl::pf17::pmr::memory_resource*>(udpard->user_reference);
    if (nullptr == res)
    {
        return nullptr;
    }
    else
    {
        return res->allocate(amount);
    }
}

/// @brief Used to free heap space used by udpard
/// @param[in] udpard Instance of udpard
/// @param[in] amount Area of memory
inline void udpardMemFree(UdpardInstance* const udpard, void* const pointer)
{
    cetl::pf17::pmr::memory_resource* res = static_cast<cetl::pf17::pmr::memory_resource*>(udpard->user_reference);
    if(nullptr !=res)
    {
        // there's no guarantee that this will work because the size is 0. See
        // https://github.com/OpenCyphal-Garage/libudpard/issues/28 for fix.
        res->deallocate(pointer, 0);
    }
}

#endif  // POSIX_LIBCYPHAL_TYPES_UDPARD_HEAP_HPP_INCLUDED
