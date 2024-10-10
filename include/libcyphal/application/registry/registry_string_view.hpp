/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_REGISTRY_STRING_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_REGISTRY_STRING_HPP_INCLUDED

#include <cetl/cetl.hpp>

#include <uavcan/_register/Name_1_0.hpp>

#include <cstddef>
#include <cstring>

namespace libcyphal
{
namespace application
{
namespace registry
{

/// Defines the type of the register name.
///
/// Mimics `std::string_view`.
/// TODO: Consider reworking when `cetl::string_view` polyfill is available.
///
struct Name
{
    using size_type     = std::size_t;
    using value_type    = char;
    using const_pointer = const char*;

    constexpr Name() noexcept
        : data_{nullptr}
        , size_{0}
    {
    }

    constexpr Name(const char* const str, const std::size_t size) noexcept
        : data_{str}
        , size_{size}
    {
        CETL_DEBUG_ASSERT((size == 0) || (str != nullptr), "");
    }

    /// Constructs a view of the null-terminated character string pointed to by `str`,
    /// not including the terminating null character.
    ///
    /// No lint b/c this is intentional implicit conversion.
    /// No Sonar cpp:S5813: Using "strlen" or "wcslen" is security-sensitive.
    /// `str` is expected to be a C-string (null terminated).
    ///
    Name(const char* const str) noexcept  // NOLINT(google-explicit-constructor, hicpp-explicit-conversions)
        : data_{str}
        , size_{(str != nullptr) ? std::strlen(str) : 0}  // NOSONAR cpp:S5813
    {
    }

    constexpr const_pointer data() const noexcept
    {
        return data_;
    }

    constexpr size_type size() const noexcept
    {
        return size_;
    }

    constexpr bool empty() const noexcept
    {
        return size_ == 0;
    }

private:
    const_pointer data_;
    size_type     size_;

};  // Name

/// Compares two views.
///
constexpr bool operator==(const Name lhs, const Name rhs) noexcept
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }
    if (lhs.empty())
    {
        return true;
    }
    return std::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}

inline uavcan::_register::Name_1_0 makeName(const uavcan::_register::Name_1_0::allocator_type& alloc, const Name name)
{
    uavcan::_register::Name_1_0 out{alloc};
    if (!name.empty())
    {
        // TODO: Fix Nunavut to expose `ARRAY_CAPACITY` so we can use it here instead of 255 hardcode.
        constexpr std::size_t NameCapacity = 255U;
        out.name.resize(std::min(name.size(), NameCapacity));

        // No Sonar `cpp:S5356` b/c we need to name payload as raw data.
        (void) std::memmove(out.name.data(), name.data(), out.name.size());  // NOSONAR cpp:S5356
    }
    return out;
}

}  // namespace registry
}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_REGISTRY_NAME_HPP_INCLUDED
