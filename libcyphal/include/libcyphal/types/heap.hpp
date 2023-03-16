/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Heap interface for *ards

#ifndef LIBCYPHAL_TYPES_HEAP_HPP_INCLUDED
#define LIBCYPHAL_TYPES_HEAP_HPP_INCLUDED

#include <cstddef>
#include "libcyphal/types/status.hpp"

namespace libcyphal
{

/// @brief Interface for a heap implementation
/// @todo Come up with a runtime complexity the implementer must meet or improve on and document
class Heap
{
public:
    /// @brief Initialization sequence for the heap
    /// @todo Come up with a runtime complexity the implementer must meet or improve on and document
    /// @return status of initialization
    virtual Status initialize() = 0;

    /// @brief Gets a void pointer to an instance of the heap of choice
    /// @return void pointer to heap instance
    virtual void* getInstance() const noexcept = 0;

    /// @brief Gets the heap size
    /// @return retrieves heap size
    virtual std::size_t getHeapSize() const noexcept = 0;

protected:
    ~Heap() = default;
};

}  // namespace libcyphal

#endif  // LIBCYPHAL_TYPES_HEAP_HPP_INCLUDED
