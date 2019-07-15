/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/**
 * @file
 * Optional utilites to provide introspection into libuavcan
 * objects and to provide metrics for library performance.
 * Facilities defined in this header can be compiled out of
 * production code and should be used for debugging or targeted
 * testing activities.
 */

#ifndef LIBUAVCAN_INTROSPECTION_HPP_INCLUDED
#define LIBUAVCAN_INTROSPECTION_HPP_INCLUDED

#include "libuavcan/libuavcan.hpp"

#ifndef LIBUAVCAN_INTROSPECTION_TRACE_ENABLE
#    define LIBUAVCAN_INTROSPECTION_TRACE_ENABLE 0
#endif

/**
 * Enable runtime asserts within libuavcan.
 *
 * Obviously, this is not a reccommended thing to turn on in production code but
 * these asserts are used for some library tests and may help diagnose bugs if
 * enabled in a special build of your software.
 */
#ifndef LIBUAVCAN_INTROSPECTION_ENABLE_ASSERT
#    define LIBUAVCAN_INTROSPECTION_ENABLE_ASSERT 0
#endif

#if LIBUAVCAN_INTROSPECTION_TRACE_ENABLE

#    include <cstdio>

#    define LIBUAVCAN_TRACEF(src, fmt, ...) std::printf("%s: " fmt "\r\n", src, __VA_ARGS__)
#    define LIBUAVCAN_TRACE(src, msg) std::printf("%s: " msg "\r\n", src)

#else

#    define LIBUAVCAN_TRACEF(...) ((void) 0)
#    define LIBUAVCAN_TRACE(...) ((void) 0)

#endif

#if LIBUAVCAN_INTROSPECTION_ENABLE_ASSERT
#    include <cassert>
#    define LIBUAVCAN_ASSERT(statement) \
        {                               \
            assert(statement);          \
        }
#else
#    define LIBUAVCAN_ASSERT(...) ((void) 0)
#endif

#endif  // LIBUAVCAN_INTROSPECTION_HPP_INCLUDED
