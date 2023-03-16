/// @file
/// Example of creating a libcyphal node in your project.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#include "libcyphal/libcyphal.hpp"
#include <iostream>
#include <string>
#include <algorithm>
#include <type_traits>

#include <gtest/gtest.h>

TEST(example_01_hello_world, snippit_0)
{
//! [snippit_0]
    // to include in doxygen just do:
    // @snippet{trimleft} example_01_hello_world.cpp snippit_0
    std::cout << "Hello World" << std::endl;
//! [snippit_0]
}
