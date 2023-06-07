/// @file
/// IPv4 Address class and utilities.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_TRANSPORT_IP_V4_ADDRESS_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_IP_V4_ADDRESS_HPP_INCLUDED

#include <cstdint>
#include <algorithm>
#include <cstring>

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

/// The Address class represents an IPv4 Address.
/// The Address class provides functionality for storing, retrieve, and manipulating IPv4 addresses.
/// It supports the IPv4 format.
class Address final
{
public:
    /// Address Default constructor
    constexpr Address() noexcept
        : Address(0u, 0u, 0u, 0u)
    {
    }

    /// Explicit Parameter Constructor as single unsigned 32 bit value (chaining)
    /// @param[in] address The 32-bit representation of the IPv4 Address
    constexpr explicit Address(AddressType address) noexcept
        : Address{Octet((address & 0xFF000000u) >> 24u),
                  Octet((address & 0x00FF0000u) >> 16u),
                  Octet((address & 0x0000FF00u) >> 8u),
                  Octet((address & 0x000000FFu) >> 0u)}
    {
    }

    /// Explicit Parameter Constructor as classical four part address
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

    /// Address Destructor
    ~Address() noexcept = default;

    /// Copy Constructor for Address
    /// @param[in] other Address to copy from
    constexpr Address(const Address& other) noexcept
        : Address(other.a_, other.b_, other.c_, other.d_)
    {
    }

    /// Move Constructor for Address
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

    /// Copy Assignment Operator for Address
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

    /// Move Assignment Operator for Address
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

    /// Address Equality Check
    /// @param[in] other The Address class to compare against
    /// @return Whether the Addresses being compared are equal
    constexpr bool operator==(const Address& other) const noexcept
    {
        return (AddressType(*this) == AddressType(other));
    }

    /// Address Inequality Check
    /// @param[in] other The Address class to compare against
    /// @return Whether the Addresses being compared are not equal
    constexpr bool operator!=(const Address& other) const noexcept
    {
        return not operator==(other);
    }

    /// Checks if address is a localhost address
    /// @return true when on the 127 subnet (localhost)
    constexpr bool isLocal() const noexcept
    {
        return (a_ == 0b0111'1111);
    }

    /// Checks if address is a multicast address
    /// @return true when a multicast address
    constexpr bool isMulticast() const noexcept
    {
        return ((a_ & 0xF0) == 0b1110'0000);
    }

    /// Operator to handle comparison with a 32-bit integer
    /// @return 32-bit integer representation of the IPv4 Address
    constexpr explicit operator AddressType() const noexcept
    {
        return static_cast<AddressType>((a_ << 24u) | (b_ << 16u) | (c_ << 8u) | (d_ << 0u));
    }

    /// Checks if address is valid and not the default route or limited broadcast
    /// @return true if the address is not the default route or the limited broadcast
    constexpr bool isValid() const noexcept
    {
        return operator!=(Address{0u, 0u, 0u, 0u}) and operator!=(Address{255u, 255u, 255u, 255u});
    }

    /// Converts Address into a 32-bit integer
    /// @return Address represented as a 32-bit integer
    constexpr AddressType asInteger() const noexcept
    {
        return static_cast<AddressType>((a_ << 24u) | (b_ << 16u) | (c_ << 8u) | (d_ << 0u));
    }

    /// Converts a given string with (presumably) integers into an Octet.
    /// @param[in] octet The string to convert into an Octet.
    /// @param[in] octet_str_len The length of the octet argument.
    /// @return The Octet representation of the given string. 0 is returned if octet is nullptr
    ///         and 255 if the value is greater than 255. All other values between 0 and 255 are
    ///         interpreted by strtoul.
    static inline Octet octetFromBase10String(const char* octet, std::size_t octet_str_len)
    {
        static constexpr std::size_t MAX_OCTET_STR_LEN = 16u;
        if (nullptr == octet)
        {
            return 0u;
        }
        std::size_t clamped_string_len = std::min(octet_str_len, MAX_OCTET_STR_LEN);
        char        conversion_buffer[MAX_OCTET_STR_LEN + 1];
        octet = &octet[octet_str_len - clamped_string_len];
        strncpy(conversion_buffer, octet, clamped_string_len);
        char* end = &conversion_buffer[clamped_string_len];
        *end      = '\0';
        return static_cast<Octet>(std::min(255ul, std::strtoul(conversion_buffer, &end, 10)));
    }

    /// Converts a string into an IPV4 address. The string must be in the format
    /// "xxx.xxx.xxx.xxx" where each xxx is an value between 0 and 255 in base-10
    /// (decimal). For example:
    /// ```
    ///     "192.168.0.10" // Four decimal octets
    /// ```
    /// @param[in] address      Address as a string.
    /// @param[in] address_len  The length of the address argument.
    /// @return IPV4 address
    static inline Address addressFromString(const char* address, std::size_t address_len)
    {
        if (nullptr == address)
        {
            return Address();
        }
        std::size_t octet_index = 0;
        Octet       octets[4]   = {0, 0, 0, 0};
        const char* last        = address;

        const char* i = std::strchr(last, '.');
        while (i != nullptr && octet_index < 3 && i < &address[address_len])
        {
            octets[octet_index++] = octetFromBase10String(last, static_cast<std::size_t>(i - last));
            last                  = i + 1;
            i                     = std::strchr(last, '.');
        }
        if (octet_index < 4 && last < &address[address_len])
        {
            octets[octet_index++] = octetFromBase10String(last, address_len - static_cast<std::size_t>(last - address));
        }
        return Address(octets[0], octets[1], octets[2], octets[3]);
    }

    /// Converts a string into an IPV4 address. The string must be in the format
    /// "xxx.xxx.xxx.xxx" where each xxx is an value between 0 and 255 in base-10
    /// (decimal). For example:
    /// ```
    ///     "192.168.0.10" // Four decimal octets
    /// ```
    /// @param[in] address          Address as a null-terminated string.
    /// @return IPV4 address
    static inline Address addressFromString(const char* address)
    {
        if (nullptr == address)
        {
            return addressFromString(address, 0);
        }
        else
        {
            return addressFromString(address, std::strlen(address));
        }
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
