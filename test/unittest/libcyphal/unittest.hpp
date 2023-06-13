/// @file
/// Common header for all libcyphal unittests.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#ifndef LIBCYPHAL_UNITTEST_HPP_INCLUDED
#define LIBCYPHAL_UNITTEST_HPP_INCLUDED

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "libcyphal/libcyphal.hpp"

#include <ostream>

namespace libcyphal
{
///
/// @namespace unittest
/// Namespace for all libcyphal unit tests and test utilities.
///
namespace unittest
{
///
/// Helper template to print a given object as a series of bytes to a given ostream.
///
/// @tparam  ObjectType  The type of the object to print.
/// @param   object      A reference to the object to print.
/// @param   out         The stream to print to.
///
template <typename ObjectType>
void PrintObjectAsBytes(const ObjectType& object, std::ostream& out)
{
    ::testing::internal::PrintBytesInObjectTo(reinterpret_cast<const unsigned char*>(&object),
                                              sizeof(ObjectType),
                                              &out);
}
}  // namespace unittest
}  // namespace libcyphal

#endif  // LIBCYPHAL_UNITTEST_HPP_INCLUDED
