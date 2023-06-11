/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Allocator/Free functions for canard

#ifndef POSIX_LIBCYPHAL_TYPES_CANARD_HEAP_HPP_INCLUDED
#define POSIX_LIBCYPHAL_TYPES_CANARD_HEAP_HPP_INCLUDED

#include <cstddef>
#include <canard.h>

#include "cetl/pf17/memory_resource.hpp"

/// @brief Used to pass in allocation function to canard
/// @param[in] canard Instance of canard
/// @param[in] amount Amount of memory to allocate
/// @return pointer to allocated fragment
inline void* canardMemAllocate(CanardInstance* const canard, const std::size_t amount)
{
    cetl::pf17::pmr::memory_resource* resource = static_cast<cetl::pf17::pmr::memory_resource*>(canard->user_reference);
    if (nullptr == resource)
    {
        return nullptr;
    }
    else
    {
        return resource->allocate(amount);
    }
}

/// @brief Used to free heap space usedby canard
/// @param[in] canard Instance of canard
/// @param[in] amount area of memory
inline void canardMemFree(CanardInstance* const canard, void* const pointer)
{
    cetl::pf17::pmr::memory_resource* resource = static_cast<cetl::pf17::pmr::memory_resource*>(canard->user_reference);
    if (nullptr != resource)
    {
        // TODO: fix when https://github.com/OpenCyphal/libcanard/issues/216 is fixed.
        resource->deallocate(pointer, 0);
    }
}

#endif  // POSIX_LIBCYPHAL_TYPES_CANARD_HEAP_HPP_INCLUDED
