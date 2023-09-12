/// @file
/// Example of using the dynamic-extent specialization of cetl::span.
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

int main()
{
//! [main]
    std::string greeting{"Hello Dynamic World."};
    cetl::pf20::span<const char> dynamic{greeting.c_str(), 13};
    auto print = [](const char c) { std::cout << c; };

    // Print just the characters in the span...
    std::for_each(dynamic.begin(), dynamic.end(), print);
    std::cout << std::endl;

    // or...
    std::string substring{dynamic.begin(), dynamic.size()};
    std::cout << substring << std::endl;
//! [main]

    return 0;
}
