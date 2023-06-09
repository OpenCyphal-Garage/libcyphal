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

/// We'll create something like this in Nunavut so I'm just providing some foreshadowing here:
/// @tparam T
template <typename T>
struct dsdl_traits
{
    static constexpr std::size_t extent() noexcept
    {
        return T::_traits_::ExtentBytes;
    }

    static constexpr std::size_t align() noexcept
    {
        return alignof(T);
    }
};

using byte                      = cetl::pf17::byte;
using monotonic_buffer_resource = cetl::pf17::pmr::deviant::basic_monotonic_buffer_resource;
template <typename T>
using polymorphic_allocator = cetl::pf17::pmr::polymorphic_allocator<T>;

constexpr std::size_t foo_count_ = 24u;
using FooStorageType             = std::aligned_storage<dsdl_traits<example::foo_1_0>::extent(), dsdl_traits<example::foo_1_0>::align()>;
FooStorageType foo_storage_[foo_count_];
FooStorageType input_buffer_[1];

TEST(example_01_serialize_foo, snippit_0)
{
    //! [snippit_0]
    // to include in doxygen just do:
    // @snippet{trimleft} example_01_hello_world.cpp snippit_0

    // Here we're using CETL's VLA to store a bunch of our foo instances.Foo is a type we defined in
    // dsd/example/foo.1.0.dsdl

    monotonic_buffer_resource               mr{&foo_storage_[0], sizeof(FooStorageType::type) * foo_count_};
    polymorphic_allocator<example::foo_1_0> alloc(&mr);
    cetl::VariableLengthArray<example::foo_1_0, decltype(alloc)> vla{alloc};

    // You'll note that CETL's PMR extends the C++ standard to support max_size so this next line would
    // not work if you were using C++17's standard library:
    std::cout << "We have storage for " << vla.max_size() << " example::foo_1_0 objects." << std::endl;

    vla.reserve(1);

    std::cout << "We have capacity for " << vla.capacity() << " objects." << std::endl;

    vla.emplace_back(example::foo_1_0{});

    // This is all kinds of badness. We need to support byte instead of uint8 (e.g. some DSP's have 16 bit bytes)
    // and we need to support CETL span, and we need to allow uninitialized memory regions to get passed in without
    // needing reinterpret_cast by the caller, etc.
    nunavut::support::bitspan fake_input_buffer{reinterpret_cast<std::uint8_t*>(input_buffer_), dsdl_traits<example::foo_1_0>::extent(), 0U};
    if (example::serialize(vla[0], fake_input_buffer))
    {
        std::cout << "Cool. You know have an in-memory representation of your foo object!" << std::endl;
    }
    else
    {
        std::cout << "Hmmmmm." << std::endl;
    }

    //! [snippit_0]
}
