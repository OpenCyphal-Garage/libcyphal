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

#define __STDC_FORMAT_MACROS

#include <cstdint>
#include <cinttypes>
#include <limits>
#include <type_traits>

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
 * Negative values are errors. 1 is a nominal result and values > 1 are also considered
 * nominal results. The meaning of a given result that is < or > 1 is different for each
 * function and each function must document this semantic.
 */
using Result = std::int_fast8_t;

/**
 * @namespace results
 * See the {@link result_mnemonics Result Mnemonics} group for documentation.
 */
namespace results
{
/**
 * @defgroup result_mnemonics Result Mnemonics
 *
 * These Result values are provided as mnemonics for interpreting result values but their use is
 * not required. The definitions are fixed and libuavcan will not remap these values.
 *
 * Greather-than or equal to 1 is a successful result.
 * Less-than 1 is a un-successful result.
 * @{
 */

/**
 * > 0 are success values for libuavcan results.
 */
constexpr Result success = 1;

/**
 * Some parts of a non-atomic operation completed successfully but other parts failed.
 * This result will only be used where additional information about the failure parts
 * can allow the caller to recover.
 */
constexpr Result success_partial = 2;

/**
 * The operation didn't do anything but no failures occurred. For example, this would be
 * returned for a read operation that read nothing successfully.
 */
constexpr Result success_nothing = 3;

/**
 * An operation failed because a buffer was full. For some operations this implies
 * that trying again with the same input can be successful.
 */
constexpr Result buffer_full = 0;

/**
 * A generic failure.
 */
constexpr Result failure = -1;

/**
 * One or more parameters provided to a function were invalid.
 */
constexpr Result bad_argument = -2;

/**
 * The operation experienced an internal inconsistency or an unexpected
 * result from a lower layer.
 */
constexpr Result unknown_internal_error = -3;

/**
 * An operation failed because there was inadequate memory available.
 */
constexpr Result out_of_memory = -4;

/**
 * @}
 */

}  // namespace results
}  // namespace libuavcan

#endif  // LIBUAVCAN_HPP_INCLUDED
