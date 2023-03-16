/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Common CAN Extended Filter definitions

#ifndef LIBCYPHAL_MEDIA_CAN_FILTER_HPP_INCLUDED
#define LIBCYPHAL_MEDIA_CAN_FILTER_HPP_INCLUDED

#include <cstdint>
#include "libcyphal/media/can/identifier.hpp"

namespace libcyphal
{
namespace media
{
namespace can
{

/// CAN 2.0b Identifier Filter Object
struct Filter
{
    /// Comparing two Filters for equality
    inline bool operator==(const Filter& other) noexcept
    {
        return (raw.getID() == other.raw.getID()) and (mask == other.mask);
    }

    /// Comparing two Filters for inequality
    inline bool operator!=(const Filter& other) noexcept
    {
        return not operator==(other);
    }

    /// The value to use for a Full Extended Mask
    constexpr static std::uint32_t FullExtMask = extended::IDMask;

    /// The value to use for a Full Standard Mask
    constexpr static std::uint32_t FullStdMask = (standard::IDMask << RawIdentifier::extended_bits);

    // PUBLIC DATA MEMBERS
    RawIdentifier raw;   //!< The Raw Identifier which could be extended or standard
    std::uint32_t mask;  //!< The entire 32 bit mask to select Any portion of the ID.
};

}  // namespace can
}  // namespace media
}  // namespace libcyphal

#endif  // LIBCYPHAL_MEDIA_CAN_FILTER_HPP_INCLUDED
