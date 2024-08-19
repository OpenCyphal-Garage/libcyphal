/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_SHARED_OBJECT_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_SHARED_OBJECT_HPP_INCLUDED

#include <cetl/cetl.hpp>

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
    virtual void release() noexcept
    {
        CETL_DEBUG_ASSERT(ref_count_ > 0, "");
        --ref_count_;
    }

protected:
    /// @brief Gets current value of the reference count.
    ///
    std::size_t getRefCount() const noexcept
    {
        return ref_count_;
    }

private:
    // MARK: Data members:

    std::size_t ref_count_{0};

};  // SharedObject

}  // namespace detail
}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_SHARED_OBJECT_HPP_INCLUDED
