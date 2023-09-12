/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Heap interface for *ards

#ifndef POSIX_LIBCYPHAL_TYPES_O1_HEAP_HPP_INCLUDED
#define POSIX_LIBCYPHAL_TYPES_O1_HEAP_HPP_INCLUDED

#include <cassert>
#include <cstddef>
#include <o1heap.h>
#include <udpard.h>
#include <libcyphal/transport/message.hpp>
#include <libcyphal/types/heap.hpp>
#include <libcyphal/types/status.hpp>

namespace libcyphal
{

/// @brief Uses O1Heap
/// @todo Figure out whether to create a runtime heap for the user given a size
class O1Heap final : public Heap
{
public:
    /// @brief Default parameters for heap
    /// @param[in] heap_area The array of the heap
    /// @param[in] heap_size Size of the heap array
    explicit O1Heap(std::uint8_t* heap_area, std::size_t heap_size)
        : Heap()
        , heap_{o1heapInit(heap_area, heap_size)}
        , heap_size_{heap_size}
    {
    }

    /// @brief Initialize the heap
    /// @return Status of heap initialization
    Status initialize() override
    {
        return (nullptr == heap_) ? ResultCode::Failure : ResultCode::Success;
    }

    /// @brief Gets instance of Heap
    /// @return void pointer to heap
    void* getInstance() const noexcept override
    {
        return heap_;
    }

    /// @brief Retrieves heap size that was initialized
    /// @return the size of the heap
    std::size_t getHeapSize() const noexcept override
    {
        return heap_size_;
    }

private:
    O1HeapInstance* heap_;
    std::size_t     heap_size_;
};

}  // namespace libcyphal

#endif  // POSIX_LIBCYPHAL_TYPES_HEAP_HPP_INCLUDED
