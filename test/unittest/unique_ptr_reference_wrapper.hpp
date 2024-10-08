/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_UNIQUE_PTR_REFERENCE_WRAPPER_HPP_INCLUDED
#define LIBCYPHAL_UNIQUE_PTR_REFERENCE_WRAPPER_HPP_INCLUDED

#include <libcyphal/types.hpp>

namespace libcyphal
{

template <typename Interface, typename Reference, typename DerivedWrapper>
struct UniquePtrReferenceWrapper : Interface
{
    struct Spec : detail::UniquePtrSpec<Interface, DerivedWrapper>
    {};

    explicit UniquePtrReferenceWrapper(Reference& reference)
        : reference_{reference}
    {
    }

    UniquePtrReferenceWrapper(const UniquePtrReferenceWrapper& other)          = delete;
    UniquePtrReferenceWrapper(UniquePtrReferenceWrapper&&) noexcept            = delete;
    UniquePtrReferenceWrapper& operator=(const UniquePtrReferenceWrapper&)     = delete;
    UniquePtrReferenceWrapper& operator=(UniquePtrReferenceWrapper&&) noexcept = delete;

    virtual ~UniquePtrReferenceWrapper()
    {
        reference_.deinit();
    }

    Reference& reference() const
    {
        return reference_;
    }

private:
    Reference& reference_;

};  // UniquePtrReferenceWrapper

}  // namespace libcyphal

#endif  // LIBCYPHAL_UNIQUE_PTR_REFERENCE_WRAPPER_HPP_INCLUDED
