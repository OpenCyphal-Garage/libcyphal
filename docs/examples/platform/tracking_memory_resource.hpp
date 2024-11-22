/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef EXAMPLE_PLATFORM_TRACKING_MEMORY_RESOURCE_HPP_INCLUDED
#define EXAMPLE_PLATFORM_TRACKING_MEMORY_RESOURCE_HPP_INCLUDED

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <algorithm>
#include <cstddef>
#include <ios>
#include <ostream>
#include <vector>

namespace example
{
namespace platform
{

class TrackingMemoryResource final : public cetl::pmr::memory_resource
{
public:
    struct Allocation final
    {
        std::size_t size;
        void*       pointer;

        friend void PrintTo(const Allocation& alloc, std::ostream* os)
        {
            *os << "\n{ptr=0x" << std::hex << alloc.pointer << ", size=" << std::dec << alloc.size << "}";
        }
    };

    // NOLINTBEGIN
    std::vector<Allocation>     allocations{};
    std::size_t                 allocated_bytes{0};
    std::size_t                 max_allocated_bytes{0};
    std::size_t                 total_allocated_bytes{0};
    std::size_t                 total_deallocated_bytes{0};
    cetl::pmr::memory_resource* memory_{cetl::pmr::get_default_resource()};
    // NOLINTEND

private:
    // MARK: cetl::pmr::memory_resource

    void* do_allocate(std::size_t size_bytes, std::size_t alignment) override
    {
        if (alignment > alignof(std::max_align_t))
        {
#if defined(__cpp_exceptions)
            throw std::bad_alloc();
#endif
            return nullptr;
        }

        void* ptr = memory_->allocate(size_bytes, alignment);

        total_allocated_bytes += size_bytes;
        allocations.push_back({size_bytes, ptr});

        allocated_bytes += size_bytes;
        max_allocated_bytes = std::max(max_allocated_bytes, allocated_bytes);

        return ptr;
    }

    void do_deallocate(void* ptr, std::size_t size_bytes, std::size_t alignment) override
    {
        CETL_DEBUG_ASSERT((nullptr != ptr) || (0 == size_bytes), "");

        if (nullptr != ptr)
        {
            auto prev_alloc = std::find_if(allocations.cbegin(), allocations.cend(), [ptr](const auto& alloc) {
                return alloc.pointer == ptr;
            });
            CETL_DEBUG_ASSERT(prev_alloc != allocations.cend(), "");
            if (prev_alloc != allocations.cend())
            {
                CETL_DEBUG_ASSERT(prev_alloc->size == size_bytes, "");
                allocations.erase(prev_alloc);
            }
            total_deallocated_bytes += size_bytes;
        }

        memory_->deallocate(ptr, size_bytes, alignment);

        allocated_bytes -= size_bytes;
    }

#if (__cplusplus < CETL_CPP_STANDARD_17)

    void* do_reallocate(void*       ptr,
                        std::size_t old_size_bytes,
                        std::size_t new_size_bytes,
                        std::size_t alignment) override
    {
        CETL_DEBUG_ASSERT((nullptr != ptr) || (0 == old_size_bytes), "");

        if (nullptr != ptr)
        {
            auto prev_alloc = std::find_if(allocations.cbegin(), allocations.cend(), [ptr](const auto& alloc) {
                return alloc.pointer == ptr;
            });
            CETL_DEBUG_ASSERT(prev_alloc != allocations.cend(), "");
            if (prev_alloc != allocations.cend())
            {
                CETL_DEBUG_ASSERT(prev_alloc->size == old_size_bytes, "");
                allocations.erase(prev_alloc);
            }
            total_allocated_bytes -= old_size_bytes;
        }

        auto* const new_ptr = memory_->reallocate(ptr, old_size_bytes, new_size_bytes, alignment);

        total_allocated_bytes += new_size_bytes;
        allocations.push_back({new_size_bytes, new_ptr});

        allocated_bytes -= old_size_bytes;
        allocated_bytes += new_size_bytes;
        max_allocated_bytes = std::max(max_allocated_bytes, allocated_bytes);

        return new_ptr;
    }

#endif

    bool do_is_equal(const cetl::pmr::memory_resource& rhs) const noexcept override
    {
        return (&rhs == this);
    }

};  // TrackingMemoryResource

}  // namespace platform
}  // namespace example

#endif  // EXAMPLE_PLATFORM_TRACKING_MEMORY_RESOURCE_HPP_INCLUDED
