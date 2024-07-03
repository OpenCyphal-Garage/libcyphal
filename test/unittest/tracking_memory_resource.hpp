/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRACKING_MEMORY_RESOURCE_HPP_INCLUDED
#define LIBCYPHAL_TRACKING_MEMORY_RESOURCE_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>

#include <algorithm>
#include <cstddef>
#include <ios>
#include <ostream>
#include <vector>

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
    std::size_t                 total_allocated_bytes   = 0;
    std::size_t                 total_deallocated_bytes = 0;
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

        return ptr;
    }

    void do_deallocate(void* ptr, std::size_t size_bytes, std::size_t alignment) override
    {
        auto prev_alloc = std::find_if(allocations.cbegin(), allocations.cend(), [ptr](const auto& alloc) {
            return alloc.pointer == ptr;
        });
        if (prev_alloc != allocations.cend())
        {
            allocations.erase(prev_alloc);
        }
        total_deallocated_bytes += size_bytes;

        memory_->deallocate(ptr, size_bytes, alignment);
    }

#if (__cplusplus < CETL_CPP_STANDARD_17)

    void* do_reallocate(void*       ptr,
                        std::size_t old_size_bytes,
                        std::size_t new_size_bytes,
                        std::size_t alignment) override
    {
        total_allocated_bytes -= old_size_bytes;
        total_allocated_bytes += new_size_bytes;

        return memory_->reallocate(ptr, old_size_bytes, new_size_bytes, alignment);
    }

#endif

    bool do_is_equal(const cetl::pmr::memory_resource& rhs) const noexcept override
    {
        return (&rhs == this);
    }

};  // TrackingMemoryResource

#endif  // LIBCYPHAL_TRACKING_MEMORY_RESOURCE_HPP_INCLUDED
