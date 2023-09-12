/// @file
/// Example of using cetl::span.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#include "cetl/pf20/span.hpp"
#include <iostream>
#include <string>
#include <algorithm>

//! [global]
template<typename T, std::size_t Extent>
std::ostream& operator<<(std::ostream& os, const cetl::pf20::span<T, Extent>& sp)
{
    std::for_each(sp.begin(), sp.end(), [&os](const char c) { os << c; });
    return os;
}
//! [global]

int main()
{
//! [main]
    constexpr const char* greeting = "Hello Static World";
    std::cout << cetl::pf20::span<const char, 12>{greeting, 12} << std::endl;
//! [main]
    return 0;
}
