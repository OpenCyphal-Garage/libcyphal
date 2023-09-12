/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Allocator/Free functions for canard

#ifndef POSIX_LIBCYPHAL_TYPES_CANARD_HEAP_HPP_INCLUDED
#define POSIX_LIBCYPHAL_TYPES_CANARD_HEAP_HPP_INCLUDED

#include <cstddef>
#include <canard.h>
#include <o1heap.h>

/// @brief Used to pass in allocation function to canard
/// @param[in] canard Instance of canard
/// @param[in] amount Amount of memory to allocate
/// @return pointer to allocated fragment
inline void* canardMemAllocate(CanardInstance* const canard, const std::size_t amount)
{
    // canard instance holds the reference to its 01heap instance in the 'user_reference' field
    O1HeapInstance* heap = static_cast<O1HeapInstance*>(canard->user_reference);
    return o1heapAllocate(heap, amount);
}

/// @brief Used to free heap space usedby canard
/// @param[in] canard Instance of canard
/// @param[in] amount area of memory
inline void canardMemFree(CanardInstance* const canard, void* const pointer)
{
    // canard instance holds the reference to its 01heap instance in the 'user_reference' field
    O1HeapInstance* heap = static_cast<O1HeapInstance*>(canard->user_reference);
    o1heapFree(heap, pointer);
}

#endif  // POSIX_LIBCYPHAL_TYPES_CANARD_HEAP_HPP_INCLUDED
