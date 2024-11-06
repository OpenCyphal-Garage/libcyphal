/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_VERIFICATION_UTILITIES_HPP_INCLUDED
#define LIBCYPHAL_VERIFICATION_UTILITIES_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

#include <nunavut/support/serialization.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <vector>

namespace libcyphal
{
namespace verification_utilities
{

inline constexpr cetl::byte b(std::uint8_t b)
{
    return static_cast<cetl::byte>(b);
}

inline void fillIotaBytes(const cetl::span<cetl::byte> span, const cetl::byte init)
{
    std::iota(reinterpret_cast<std::uint8_t*>(span.data()),                // NOLINT
              reinterpret_cast<std::uint8_t*>(span.data() + span.size()),  // NOLINT
              static_cast<std::uint8_t>(init));
}

template <std::size_t N>
std::array<cetl::byte, N> makeIotaArray(const cetl::byte init)
{
    std::array<cetl::byte, N> arr{};
    fillIotaBytes(arr, init);
    return arr;
}

template <std::size_t N>
std::array<cetl::span<const cetl::byte>, 1> makeSpansFrom(const std::array<cetl::byte, N>& payload)
{
    return {payload};
}

template <std::size_t N1, std::size_t N2>
std::array<cetl::span<const cetl::byte>, 2> makeSpansFrom(const std::array<cetl::byte, N1>& payload1,
                                                          const std::array<cetl::byte, N2>& payload2)
{
    return {payload1, payload2};
}

template <typename T>
static bool tryDeserialize(T& obj, const cetl::span<const cetl::span<const cetl::byte>> fragments)
{
    std::vector<cetl::byte> bytes;
    for (const auto& fragment : fragments)
    {
        std::copy_n(fragment.begin(), fragment.size(), std::back_inserter(bytes));
    }
    const auto* const buffer = reinterpret_cast<const std::uint8_t*>(bytes.data());  // NOLINT

    const nunavut::support::const_bitspan bitspan{buffer, bytes.size()};
    return deserialize(obj, bitspan);
}

}  // namespace verification_utilities
}  // namespace libcyphal

#endif  // LIBCYPHAL_VERIFICATION_UTILITIES_HPP_INCLUDED
