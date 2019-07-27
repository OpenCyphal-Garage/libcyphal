/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/** @file
 * Common header for all Libuavcan Validation Suite tests.
 */
#ifndef LIBUAVCAN_LVS_HPP_INCLUDED
#define LIBUAVCAN_LVS_HPP_INCLUDED

#include "gtest/gtest.h"
#include "libuavcan/libuavcan.hpp"

#include <ostream>

/**
 * @namespace lvs
 * Namespace for all Libuavcan Validation Suite types and utilities.
 *
 * See the @ref LVSGuide for the LVS documentation.
 */
namespace lvs
{
/**
 * Helper template to print a given object as a series of bytes to a given ostream.
 *
 * @tparam  ObjectType  The type of the object to print.
 * @param   object      A reference to the object to print.
 * @param   out         The stream to print to.
 */
template <typename ObjectType>
void PrintObjectAsBytes(const ObjectType& object, std::ostream& out)
{
    ::testing::internal2::PrintBytesInObjectTo(reinterpret_cast<const unsigned char*>(&object),
                                               sizeof(ObjectType),
                                               &out);
}
}  // namespace lvs

#if LIBUAVCAN_ENABLE_EXCEPTIONS
/**
 * If exceptions are enabled then expands to gtest `ASSERT_THROW(statement, exception_type)`.
 * If exceptions are not enabled then expands to gtest `ASSERT_EQ(statement, no_exception_value)`.
 */
#    define LVS_ASSERT_THROW_IF_EXCEPTIONS_OR_VALUE(statement, exception_type, no_exception_value) \
        ASSERT_THROW(statement, exception_type)

#else
/**
 * If exceptions are enabled then expands to gtest `ASSERT_THROW(statement, exception_type)`.
 * If exceptions are not enabled then expands to gtest `ASSERT_EQ(statement, no_exception_value)`.
 */
#    define LVS_ASSERT_THROW_IF_EXCEPTIONS_OR_VALUE(statement, exception_type, no_exception_value) \
        ASSERT_EQ((statement), no_exception_value)

#endif

#endif  // LIBUAVCAN_LVS_HPP_INCLUDED
