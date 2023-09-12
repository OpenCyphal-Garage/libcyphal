/// @file
/// Example of AUTOSAR-14 compliant use of CETL.
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
#include <type_traits>

namespace
{

// Use CETL in this file but make it easy to switch to std:: if we upgrade
// our compiler.
constexpr std::size_t my_dynamic_extent = cetl::pf20::dynamic_extent;

template <typename T, std::size_t Extent = my_dynamic_extent>
using myspan = cetl::pf20::span<T, Extent>;

}  // end anonymous namespace

int main()
{
    std::string        greeting{"Hello Dynamic World."};
    myspan<const char> dynamic{greeting.c_str(), 13};
    auto               print = [](const char c) { std::cout << c; };

    // Print just the characters in the span...
    std::for_each(dynamic.begin(), dynamic.end(), print);
    std::cout << std::endl;

    // or...
    std::string substring{dynamic.begin(), dynamic.end()};
    std::cout << substring << std::endl;

    return 0;
}
