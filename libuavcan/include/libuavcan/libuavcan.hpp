/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/**
 * @file
 * This header should be included by the user application.
 */
#ifndef LIBUAVCAN_HPP_INCLUDED
#define LIBUAVCAN_HPP_INCLUDED

#include "libuavcan/build_config.hpp"

#include <cstdint>
#include <cinttypes>
#include <limits>
#include <type_traits>
#include <cstddef>

/**
 * @namespace libuavcan
 * The top-level namespace which contains all types, definitions, and nested
 * namespaces for libuavcan.
 */
namespace libuavcan
{
/**
 * Common return type for functions that report a result. Since libuavcan does not
 * require C++ exceptions this return type is used to signal failures within a function.
 * Negative values and zero are errors. 1 is a nominal result and values > 1 are also considered
 * nominal results.
 */
enum struct Result : std::int_fast8_t
{

    /**
     * Values greater than 0 are success values for libuavcan results.
     */
    Success = 1,

    /**
     * Some parts of a non-atomic operation completed successfully but other parts failed.
     * This result will only be used where additional information about the failure parts
     * can allow the caller to recover.
     */
    SuccessPartial = 2,

    /**
     * The operation didn't do anything but no failures occurred. For example, this would be
     * returned for a read operation that read nothing successfully.
     */
    SuccessNothing = 3,

    /**
     * No errors occurred but the operation did not complete because a timeout period was reached.
     */
    SuccessTimeout = 4,

    /**
     * An operation failed because a buffer was full. For some operations this implies
     * that trying again with the same input can be successful.
     */
    BufferFull = 0,

    /**
     * A generic failure.
     */
    Failure = -1,

    /**
     * One or more parameters provided to a function were invalid.
     */
    BadArgument = -2,

    /**
     * The operation experienced an internal inconsistency or an unexpected
     * result from a lower layer.
     */
    UnknownInternalError = -3,

    /**
     * An operation failed because there was inadequate memory available.
     */
    OutOfMemory = -4,

    /**
     * A lookup failed to find anything for the given search parameters.
     */
    NotFound = -5,

    /**
     * The operation failed because it was not implemented.
     */
    NotImplemented = -6

};

/**
 * Allows unary syntax for evaluating a result. For example:
 * @code
 *  if (!!someMethodThatReturnsResult())
 *  {
 *      // success!
 *  }
 * @endcode
 */
constexpr bool operator!(const Result& result)
{
    return (static_cast<std::underlying_type<Result>::type>(result) <= 0);
}

/**
 * Helper to evaluate that the result is a success. Equivalent to:
 * @code
 *    static_cast<std::underlying_type<Result>::type>(result) > 0
 * @endcode
 */
constexpr bool isSuccess(const Result& result)
{
    return !!result;
}

/**
 * Helper to evaluate that the result is a success. Equivalent to:
 * @code
 *    static_cast<std::underlying_type<Result>::type>(result) <= 0
 * @endcode
 */
constexpr bool isFailure(const Result& result)
{
    return !result;
}

}  // namespace libuavcan

#endif  // LIBUAVCAN_HPP_INCLUDED
