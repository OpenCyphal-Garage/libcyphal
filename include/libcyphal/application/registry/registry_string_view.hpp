/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_REGISTRY_STRING_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_REGISTRY_STRING_HPP_INCLUDED

#include <cetl/cetl.hpp>

#include <cstddef>
#include <cstring>

namespace libcyphal
{
namespace application
{
namespace registry
{

/// Mimics `std::string_view`.
/// TODO: Consider reworking when `cetl::string_view` polyfill is available.
///
struct StringView
{
    using size_type     = std::size_t;
    using value_type    = char;
    using const_pointer = const char*;

    constexpr StringView() noexcept
        : data_{nullptr}
        , size_{0}
    {
    }

    constexpr StringView(const char* const str, const std::size_t size) noexcept
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
    StringView(const char* const str) noexcept  // NOLINT(google-explicit-constructor, hicpp-explicit-conversions)
        : data_{str}
        , size_{(str != nullptr) ? std::strlen(str) : 0}  // NOSONAR cpp:S5813
    {
    }

    /// Cannot be constructed from nullptr.
    ///
    constexpr StringView(std::nullptr_t) = delete;

    constexpr bool empty() const noexcept
    {
        return size_ == 0;
    }

    constexpr size_type size() const noexcept
    {
        return size_;
    }

    constexpr const_pointer data() const noexcept
    {
        return data_;
    }

private:
    const_pointer data_;
    size_type     size_;

};  // Name

/// Compares two views.
///
constexpr bool operator==(const StringView lhs, const StringView rhs) noexcept
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

}  // namespace registry
}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_REGISTRY_NAME_HPP_INCLUDED
