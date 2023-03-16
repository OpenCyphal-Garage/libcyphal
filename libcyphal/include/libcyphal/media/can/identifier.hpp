/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Common CAN Extended Identifier definitions

#ifndef LIBCYPHAL_MEDIA_CAN_IDENTIFIER_HPP_INCLUDED
#define LIBCYPHAL_MEDIA_CAN_IDENTIFIER_HPP_INCLUDED

#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace libcyphal
{
namespace media
{
namespace can
{
namespace extended
{

/// The maximum data size in an extended frame
constexpr std::size_t MaxDataPayload = 64ul;

/// The number of bits in Extended IDs
constexpr std::uint32_t IDBits = 29ul;
constexpr std::uint32_t IDMask = ((1 << IDBits) - 1);
}  // namespace extended

namespace standard
{
/// The maximum data size in an standard frame
constexpr std::size_t MaxDataPayload = 8ul;

/// The number of bits in Standard IDs
constexpr std::uint32_t IDBits = 11ul;
constexpr std::uint32_t IDMask = ((1 << IDBits) - 1);
}  // namespace standard

/// CAN identifier structure. Is specialized into 29 bit and 11 bit variants.
template <std::size_t N>
struct Identifier
{
    static_assert(N == extended::IDBits or N == standard::IDBits, "Must either be 29 (Extended) or 11 bits (Standard)");
    constexpr static std::size_t MaxDataPayload =
        (N == extended::IDBits) ? extended::MaxDataPayload : standard::MaxDataPayload;

    // PUBLIC DATA MEMBERS
    std::uint32_t value : N;
    std::uint32_t : 32 - N;

    /// true if an extended identifier, false if standard
    constexpr bool isExtended() const noexcept;

    /// Returns the id value
    constexpr std::uint32_t getID() const noexcept
    {
        return value;
    }

    /// Comparing two ids for equality
    constexpr bool operator==(const Identifier& other) noexcept
    {
        return value == other.value;
    }

    /// Comparing two ids for inequality
    constexpr bool operator!=(const Identifier& other) noexcept
    {
        return not operator==(other);
    }
};

/// Specialized Extended ID method
template <>
constexpr bool Identifier<extended::IDBits>::isExtended() const noexcept
{
    return true;
}

/// Specialized Standard ID method
template <>
constexpr bool Identifier<standard::IDBits>::isExtended() const noexcept
{
    return false;
}

// any other types are not implemented (invalid)

namespace extended
{
/// The Extended Identifier for CAN
using Identifier = libcyphal::media::can::Identifier<extended::IDBits>;
}  // namespace extended

namespace standard
{
/// The Standard Identifier for CAN
using Identifier = libcyphal::media::can::Identifier<standard::IDBits>;
}  // namespace standard

static_assert(extended::Identifier{0ul}.isExtended(), "Must be extended type");
static_assert(not standard::Identifier{0ul}.isExtended(), "Must be standard type");

/// CAN message identifier. 29 bits are valid, bits 29-30 are ignored, 31 is a frame id type identifier
/// @note Unions can not be constexpr'd yet (or ever) sadly.
struct RawIdentifier
{
    constexpr static std::uint32_t extended_bits = 18;
    constexpr static std::uint32_t standard_bits = 11;
    constexpr static std::uint32_t type_bits     = 1;
    constexpr static std::uint32_t extended_type = 1;
    constexpr static std::uint32_t standard_type = 0;

    /// The maximal value between the two types
    constexpr static std::size_t MaxDataPayload = std::max(extended::MaxDataPayload, standard::MaxDataPayload);

    // No "constructors" so that the default non-narrowing field initializer list is the default.
    // Defining any constructor will subtract that!

    /// Returns true if the value is extended
    constexpr bool isExtended() const noexcept
    {
        return (type == extended_type);
    }

    /// Indicates an invalid ID. Standard IDs should not have extended values.
    constexpr bool isValid() const noexcept
    {
        return not((type == standard_type) and (extended != 0));
    }

    /// Returns the ID field
    std::uint32_t getID() const noexcept
    {
        if (type == extended_type)
        {
            return std::uint32_t(standard << extended_bits) | extended;
        }
        else
        {
            return standard;
        }
    }

    /// Used by external caller to set an ID
    /// @param is_extended Set any non-zero value to indicate it is an extended ID
    /// @param raw_id Either a 29 or 11 bit ID based on the other parameter.
    void setID(std::uint32_t is_extended, std::uint32_t raw_id) noexcept
    {
        type = 0x1 & ((is_extended != 0) ? extended_type : standard_type);
        if (type == extended_type)
        {
            // we don't need to remove any upper bits as the bitfields
            // will limit assignment of other bits outside the range

            // set the extended field
            extended = raw_id & ((1 << extended_bits) - 1);
            standard = standard::IDMask & (raw_id >> extended_bits);
        }
        else if (type == standard_type)
        {
            extended = 0u;
            standard = (raw_id & ((1 << standard_bits) - 1));
        }
    }

    /// Returns the appropriate mask per the type
    std::uint32_t getMask() const noexcept
    {
        if (type == extended_type)
        {
            return extended::IDMask;
        }
        else
        {
            return standard::IDMask;
        }
    }

    /// Returns a standard ID
    standard::Identifier toStandard() const noexcept
    {
        return standard::Identifier{standard};
    }

    /// Returns an Extended ID
    extended::Identifier toExtended() const noexcept
    {
        return extended::Identifier{(extended::IDMask & getID())};
    }

    /// Equality Operator
    inline bool operator==(const RawIdentifier& other) const noexcept
    {
        return (type == other.type) and (getID() == other.getID());
    }

    /// Inequality Operator
    inline bool operator!=(const RawIdentifier& other) const noexcept
    {
        return not operator==(other);
    }

    // START PUBLIC DATA MEMBERS
    std::uint32_t extended : extended_bits;
    std::uint32_t standard : standard_bits;
    std::uint32_t : 32 - type_bits - extended_bits - standard_bits;
    std::uint32_t type : type_bits;
};

/// This is just like a copy constructor, but if we define that we loose the implicit constructor which detects overflow
static constexpr RawIdentifier toRawIdentifier(const extended::Identifier& ext) noexcept
{
    std::uint32_t exid  = ext.value & ((1 << RawIdentifier::extended_bits) - 1);
    std::uint32_t stdid = (standard::IDMask & ext.value) >> RawIdentifier::extended_bits;
    return RawIdentifier{extended::IDBits & exid, standard::IDMask & stdid, RawIdentifier::extended_type};
}

/// This is just like a copy constructor, but if we define that we loose the implicit constructor which detects overflow
static constexpr RawIdentifier toRawIdentifier(const standard::Identifier& stnd) noexcept
{
    return RawIdentifier{0u, stnd.value, RawIdentifier::standard_type};
}

}  // namespace can
}  // namespace media
}  // namespace libcyphal

#endif  // LIBCYPHAL_MEDIA_CAN_IDENTIFIER_HPP_INCLUDED
