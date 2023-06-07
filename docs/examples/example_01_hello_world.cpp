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

#include "cetl/pf17/sys/memory_resource.hpp"
#include "cetl/pf17/byte.hpp"
#include "cetl/variable_length_array.hpp"

#include "example/foo_1_0.hpp"

using byte = cetl::pf17::byte;
using monotonic_buffer_resource = cetl::pf17::pmr::deviant::basic_monotonic_buffer_resource;
template<typename T>
using polymorphic_allocator = cetl::pf17::pmr::polymorphic_allocator<T>;

TEST(example_01_hello_world, snippit_0)
{
//! [snippit_0]
    // to include in doxygen just do:
    // @snippet{trimleft} example_01_hello_world.cpp snippit_0
    std::cout << "Hello World" << std::endl;

    byte storage[10];
    monotonic_buffer_resource mr{storage, sizeof(storage)};
    polymorphic_allocator<int> alloc(&mr);
    cetl::VariableLengthArray<int, polymorphic_allocator<int>> vla{alloc};

//! [snippit_0]
}
