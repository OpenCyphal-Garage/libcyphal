/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TEST_UTILITIES_HPP
#define LIBCYPHAL_TEST_UTILITIES_HPP

#include <libcyphal/types.hpp>

#include <numeric>

namespace libcyphal
{
namespace test_utilities
{

inline constexpr cetl::byte b(std::uint8_t b)
{
    return static_cast<cetl::byte>(b);
}

template <std::size_t N>
std::array<cetl::byte, N> makeIotaArray(const std::uint8_t init)
{
    std::array<cetl::byte, N> arr{};
    std::iota(reinterpret_cast<std::uint8_t*>(arr.begin()), reinterpret_cast<std::uint8_t*>(arr.end()), init);
    return arr;
}

template <std::size_t N>
std::array<cetl::span<const cetl::byte>, 1> makeSpansFrom(const std::array<cetl::byte, N>& payload)
{
    return {payload};
}

} // namespace test_utilities
} // namespace libcyphal

#endif  // LIBCYPHAL_TEST_UTILITIES_HPP
