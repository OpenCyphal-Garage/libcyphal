/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Common CAN data length value definition for CAN

#ifndef LIBCYPHAL_MEDIA_CAN_DATA_LENGTH_CODE_HPP_INCLUDED
#define LIBCYPHAL_MEDIA_CAN_DATA_LENGTH_CODE_HPP_INCLUDED

#include <array>
#include <cstddef>
#include <cstdint>

namespace libcyphal
{
namespace media
{
// The Controller Area Network namespace
namespace can
{

/// Use the DLC as an index into this array
constexpr static std::array<std::uint32_t, 16> ValidDLCToLength =
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};

/// The DLC is a 4 bit value in the CAN specification which is not a length but a value used to convert to a length
struct DataLengthCode
{
    // PUBLIC DATA MEMBERS
    std::uint32_t value : 4;  //!< Per CAN 2.0b spec
    std::uint32_t : 28;

    /// Comparing two dlc for equality
    constexpr bool operator==(const DataLengthCode& other) const noexcept
    {
        return value == other.value;
    }

    /// Comparing two dlc for inequality
    constexpr bool operator!=(const DataLengthCode& other) const noexcept
    {
        return not operator==(other);
    }

    /// Comparing two dlc for equality
    constexpr bool operator<(const DataLengthCode& other) const noexcept
    {
        return value < other.value;
    }

    /// Comparing two dlc for inequality
    constexpr bool operator>(const DataLengthCode& other) const noexcept
    {
        return value > other.value;
    }

    /// Comparing two dlc for equality
    constexpr bool operator<=(const DataLengthCode& other) const noexcept
    {
        return value <= other.value;
    }

    /// Comparing two dlc for inequality
    constexpr bool operator>=(const DataLengthCode& other) const noexcept
    {
        return value >= other.value;
    }

    /// Converts the value to a valid length in bytes
    constexpr std::size_t toLength() const noexcept
    {
        return ValidDLCToLength[value];
    }

    /// Indicates if the value is above the standard range into the Extended range.
    constexpr bool isExtended() const noexcept
    {
        return value > 8ul;
    }

    /// Clamp the value of the DLC to the standard range.
    constexpr void clampToStandard() noexcept
    {
        if (value > 8u)
        {
            value = 8u;
        }
    }
};

/// Converts a length to the nearest DLC which will fit that length.
/// @param[in] len actual length of buffer
/// @return translated data length code
/// @note Lengths over 64 will return a DLC of zero.
/// @post Check your ToLength() to make sure you have enough space!
static constexpr DataLengthCode nearestDataLengthCode(std::size_t len) noexcept
{
    for (std::uint32_t i = 0; i < ValidDLCToLength.size(); i++)
    {
        auto v = ValidDLCToLength[i];
        if (v >= len)
        {
            return DataLengthCode{(0xF & i)};
        }
    }
    return DataLengthCode{0u};
}

static_assert(DataLengthCode{11ul}.toLength() == 20ul, "Must be correct");
static_assert(nearestDataLengthCode(13ul) == DataLengthCode{10ul}, "Must be correct");
static_assert(nearestDataLengthCode(47ul) == DataLengthCode{14ul}, "Must be correct");
static_assert(nearestDataLengthCode(65ul) == DataLengthCode{0ul}, "Must be correct");

}  // namespace can
}  // namespace media
}  // namespace libcyphal

#endif  // LIBCYPHAL_MEDIA_CAN_DATA_LENGTH_CODE_HPP_INCLUDED
