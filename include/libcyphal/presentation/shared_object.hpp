/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_SHARED_OBJECT_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_SHARED_OBJECT_HPP_INCLUDED

#include "libcyphal/errors.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <cstddef>

namespace libcyphal
{
namespace presentation
{

/// Internal implementation details of the Presentation layer.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

/// @brief Defines the base class for all classes that need to be shared (using reference count).
///
class SharedObject
{
public:
    SharedObject()          = default;
    virtual ~SharedObject() = default;

    SharedObject(const SharedObject&)                = delete;
    SharedObject(SharedObject&&) noexcept            = delete;
    SharedObject& operator=(const SharedObject&)     = delete;
    SharedObject& operator=(SharedObject&&) noexcept = delete;

    /// @brief Increments the reference count.
    ///
    void retain() noexcept
    {
        ++ref_count_;
    }

    /// @brief Decrements the reference count.
    ///
    /// @return `true` if the object is no longer referenced, `false` otherwise.
    ///
    virtual bool release() noexcept
    {
        CETL_DEBUG_ASSERT(ref_count_ > 0, "");
        --ref_count_;
        return ref_count_ == 0;
    }

    /// @brief Destroys the object.
    ///
    /// Call to this method should be the last one for the object.
    /// Concrete final derived class should implement this method
    /// by calling virtual destructor, and then deallocating memory,
    /// f.e. from PMR (use `destroyWithPmr<Concrete>` helper to do this).
    ///
    virtual void destroy() noexcept = 0;

    /// @brief Helper which creates a new concrete object with the given PMR memory resource.
    ///
    /// @tparam Concrete The concrete final type of the object to create.
    /// @param concrete The pointer to the concrete object; `nullptr` if creation failed.
    ///
    template <typename Concrete, typename Failure, typename... Args>
    static Concrete* createWithPmr(cetl::pmr::memory_resource& memory,
                                   cetl::optional<Failure>&    out_failure,
                                   Args&&... args)
    {
        libcyphal::detail::PmrAllocator<Concrete> allocator{&memory};

        if (auto* const concrete = allocator.allocate(1))
        {
            allocator.construct(concrete, std::forward<Args>(args)...);
            return concrete;
        }

        out_failure = MemoryError{};
        return nullptr;
    }

    /// @brief Helper which destroys the concrete object with the given PMR memory resource.
    ///
    /// @tparam Concrete The concrete final type of the object to destroy.
    /// @param concrete The pointer to the concrete object to destroy.
    ///
    template <typename Concrete>
    static void destroyWithPmr(Concrete* const concrete, cetl::pmr::memory_resource& memory) noexcept
    {
        CETL_DEBUG_ASSERT(concrete != nullptr, "");

        libcyphal::detail::PmrAllocator<Concrete> allocator{&memory};

        // No Sonar
        // - cpp:S3432   "Destructors should not be called explicitly"
        // - cpp:M23_329 "Advanced memory management" shall not be used"
        // b/c we do our own low-level PMR management here.
        concrete->~Concrete();  // NOSONAR cpp:S3432 cpp:M23_329

        allocator.deallocate(concrete, 1);
    }

private:
    // MARK: Data members:

    std::size_t ref_count_{0};

};  // SharedObject

}  // namespace detail
}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_SHARED_OBJECT_HPP_INCLUDED
