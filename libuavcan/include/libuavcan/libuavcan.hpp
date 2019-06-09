/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/**
 * @file
 * This header should be included by the user application.
 */
#include "libuavcan/build_config.hpp"

#include <cstdint>
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
 * Negative values are errors. 0 is a nominal result and values > 0 are also considered
 * nominal results. The meaning of a given result that is < or > 0 is different for each
 * function and each function must document this semantic.
 */
using Result = std::int_fast8_t;

}  // namespace libuavcan
