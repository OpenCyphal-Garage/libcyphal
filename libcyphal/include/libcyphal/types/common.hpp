/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Common definitions used throughout Libcyphal

#ifndef LIBCYPHAL_TYPES_COMMON_HPP_INCLUDED
#define LIBCYPHAL_TYPES_COMMON_HPP_INCLUDED

#include <cstdint>
#include <array>

namespace libcyphal
{

constexpr std::uint8_t IDSize      = 16;
constexpr std::uint8_t UIDSize     = 16;
constexpr std::uint8_t BitsPerByte = 8;

using EnumType = std::int_fast8_t;
using ID       = std::array<std::int8_t, IDSize>;
using UID      = std::array<std::int8_t, UIDSize>;

}  // namespace libcyphal

#endif  // LIBCYPHAL_TYPES_COMMON_HPP_INCLUDED
