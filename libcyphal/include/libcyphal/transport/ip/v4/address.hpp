/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// A generic class representation of an IPv4 Address

#ifndef LIBCYPHAL_TRANSPORT_IP_V4_ADDRESS_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_IP_V4_ADDRESS_HPP_INCLUDED

#include <cstdint>

namespace libcyphal
{
namespace transport
{
namespace ip
{
namespace v4
{

using Octet       = std::uint8_t;
using AddressType = std::uint32_t;

/// @brief The Address class represents an IPv4 Address
/// The Address class provides functionality for storing, retrieve, and manipulating IPv4 addressess.
/// It supports the IPv4 format.
class Address final
{
public:
    /// @brief Address Default constructor
    constexpr Address() noexcept
        : Address(0u, 0u, 0u, 0u)
    {
    }

    /// @brief Explicit Parameter Constructor as single unsigned 32 bit value (chaining)
    /// @param[in] address The 32-bit representation of the IPv4 Address
    constexpr explicit Address(AddressType address) noexcept
        : Address{Octet((address & 0xFF000000u) >> 24u),
                  Octet((address & 0x00FF0000u) >> 16u),
                  Octet((address & 0x0000FF00u) >> 8u),
                  Octet((address & 0x000000FFu) >> 0u)}
    {
    }

    /// @brief Explicit Parameter Constructor as classical four part address
    /// @note For A.B.C.D address instantiate in the same order as {A,B,C,D};
    /// @param[in] a The left most octet of the IPv4 Address read from left to right as a string
    /// @param[in] b The second to the left most octet of the IPv4 Address read from left to right as a string
    /// @param[in] c The second to the right most octet of the IPv4 Address read from left to right as a string
    /// @param[in] d The right most octet of the IPv4 Address read from left to right as a string
    constexpr explicit Address(Octet a, Octet b, Octet c, Octet d) noexcept
        : d_{d}
        , c_{c}
        , b_{b}
        , a_{a}
    {
    }

    /// @brief Address Destructor
    ~Address() noexcept = default;

    /// @brief Copy Constructor for Address
    /// @param[in] other Address to copy from
    constexpr Address(const Address& other) noexcept
        : Address(other.a_, other.b_, other.c_, other.d_)
    {
    }

    /// @brief Move Constructor for Address
    /// @param[in] other Address to move from
    constexpr Address(Address&& other) noexcept
        : d_{other.d_}
        , c_{other.c_}
        , b_{other.b_}
        , a_{other.a_}
    {
        other.a_ = 0;
        other.b_ = 0;
        other.c_ = 0;
        other.d_ = 0;
    }

    /// @brief Copy Assignment Operator for Address
    /// @param[in] other Address to copy from
    /// @return Updated address if different
    constexpr Address& operator=(const Address& other) noexcept
    {
        if (this != &other)
        {
            a_ = other.a_;
            b_ = other.b_;
            c_ = other.c_;
            d_ = other.d_;
        }
        return (*this);
    }

    /// @brief Move Assignment Operator for Address
    /// @param[in] other Address to copy from
    /// @return Updated address if different
    constexpr Address& operator=(Address&& other) noexcept
    {
        if (this != &other)
        {
            a_       = other.a_;
            b_       = other.b_;
            c_       = other.c_;
            d_       = other.d_;
            other.a_ = 0;
            other.b_ = 0;
            other.c_ = 0;
            other.d_ = 0;
        }
        return (*this);
    }

    /// @brief Address Equality Check
    /// @param[in] other The Address class to compare against
    /// @return Whether the Addresses being compared are equal
    constexpr bool operator==(const Address& other) const noexcept
    {
        return (AddressType(*this) == AddressType(other));
    }

    /// @brief Address Inequality Check
    /// @param[in] other The Address class to compare against
    /// @return Whether the Addresses being compared are not equal
    constexpr bool operator!=(const Address& other) const noexcept
    {
        return not operator==(other);
    }

    /// @brief Checks if address is a localhost address
    /// @return true when on the 127 subnet (localhost)
    constexpr bool isLocal() const noexcept
    {
        return (a_ == 0b0111'1111);
    }

    /// @brief Checks if address is a multicast address
    /// @return true when a multicast address
    constexpr bool isMulticast() const noexcept
    {
        return ((a_ & 0xF0) == 0b1110'0000);
    }

    /// @brief Operator to handle comparison with a 32-bit integer
    /// @return 32-bit integer representation of the IPv4 Address
    constexpr explicit operator AddressType() const noexcept
    {
        return static_cast<AddressType>((a_ << 24u) | (b_ << 16u) | (c_ << 8u) | (d_ << 0u));
    }

    /// @brief Checks if address is valid and not the default route or limited broadcast
    /// @return true if the address is not the default route or the limited broadcast
    constexpr bool isValid() const noexcept
    {
        return operator!=(Address{0u, 0u, 0u, 0u}) and operator!=(Address{255u, 255u, 255u, 255u});
    }

    /// @brief Converts Address into a 32-bit integer
    /// @return Address represented as a 32-bit integer
    constexpr AddressType asInteger() const noexcept
    {
        return static_cast<AddressType>((a_ << 24u) | (b_ << 16u) | (c_ << 8u) | (d_ << 0u));
    }

private:
    Octet d_;
    Octet c_;
    Octet b_;
    Octet a_;
};

}  // namespace v4
}  // namespace ip
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_IP_V4_ADDRESS_HPP_INCLUDED
