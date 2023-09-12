/// @copyright Copyright (c) 2023 Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Implements common utility functions for Libcyphal

#ifndef POSIX_LIBCYPHAL_UTILS_HPP_INCLUDED
#define POSIX_LIBCYPHAL_UTILS_HPP_INCLUDED

#include <stdlib.h>
#include <cstddef>
#include <cstdint>
#include <string>

#include "libcyphal/transport/ip/v4/address.hpp"

namespace libcyphal
{
/// @brief Converts a string into an IPV4 address
/// @todo OVPG-3146: Add more error checking to AddressFromString function and don't use std::string
/// @param[in] address Address as a string
/// @return IPV4 address
inline libcyphal::transport::ip::v4::Address AddressFromString(const std::string& address) {
    std::size_t search_index{0};
    std::uint8_t octet_index{0};
    std::uint8_t a{0};
    std::uint8_t b{0};
    std::uint8_t c{0};
    std::uint8_t d{0};

    /// Loops through and parses for "." to get the octets
    for (size_t i = 0; i < address.length(); i++) {
        if (address.substr(i, 1).compare(".") == 0) {
            if (octet_index == 0) {
                a = static_cast<std::uint8_t>(std::stoul(address.substr(search_index, (i - search_index))));
            } else if (octet_index == 1) {
                b = static_cast<std::uint8_t>(std::stoul(address.substr(search_index, (i - search_index))));
            } else if (octet_index == 2) {
                c = static_cast<std::uint8_t>(std::stoul(address.substr(search_index, (i - search_index))));
            }
            search_index = i + 1;
            octet_index++;
        }
    }
    d = static_cast<std::uint8_t>(std::stoul(address.substr(search_index, (address.length() - search_index))));
    return libcyphal::transport::ip::v4::Address(a, b, c, d);
}

} // namespace libcyphal

#endif  // POSIX_LIBCYPHAL_UTILS_HPP_INCLUDED
