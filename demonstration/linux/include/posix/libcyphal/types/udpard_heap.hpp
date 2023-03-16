/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Allocator/Free functions for udpard

#ifndef POSIX_LIBCYPHAL_TYPES_UDPARD_HEAP_HPP_INCLUDED
#define POSIX_LIBCYPHAL_TYPES_UDPARD_HEAP_HPP_INCLUDED

#include <cstddef>
#include <o1heap.h>
#include <udpard.h>

/// @brief Used to pass in allocation function to udpard
/// @param[in] udpard Instance of udpard
/// @param[in] amount Amount of memory to allocate
/// @return Pointer to allocated fragment
inline void* udpardMemAllocate(UdpardInstance* const udpard, const std::size_t amount)
{
    // Udpard instance holds the reference to its 01heap instance in the 'user_reference' field
    O1HeapInstance* heap = static_cast<O1HeapInstance*>(udpard->user_reference);
    return o1heapAllocate(heap, amount);
}

/// @brief Used to free heap space used by udpard
/// @param[in] udpard Instance of udpard
/// @param[in] amount Area of memory
inline void udpardMemFree(UdpardInstance* const udpard, void* const pointer)
{
    // Udpard instance holds the reference to its 01heap instance in the 'user_reference' field
    O1HeapInstance* heap = static_cast<O1HeapInstance*>(udpard->user_reference);
    o1heapFree(heap, pointer);
}

#endif  // POSIX_LIBCYPHAL_TYPES_UDPARD_HEAP_HPP_INCLUDED
