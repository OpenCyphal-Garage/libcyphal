/// @file
/// Optional utilities to provide introspection into libcyphal
/// objects and to provide metrics for library performance.
/// Facilities defined in this header can be compiled out of
/// production code and should be used for debugging or targeted
/// testing activities.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_INTROSPECTION_HPP_INCLUDED
#define LIBCYPHAL_INTROSPECTION_HPP_INCLUDED

#include "libcyphal/libcyphal.hpp"
#if LIBCYPHAL_INTROSPECTION_ENABLE_ASSERT
#    ifndef CETL_ENABLE_DEBUG_ASSERT
#        define CETL_ENABLE_DEBUG_ASSERT
#    endif
#elif defined CETL_ENABLE_DEBUG_ASSERT
#    define LIBCYPHAL_INTROSPECTION_ENABLE_ASSERT 1
#endif

// we use CETL_DEBUG_ASSERT as the debug assert macro for libcyphal.
#include "cetl/cetl.hpp"

#ifndef LIBCYPHAL_INTROSPECTION_TRACE_ENABLE
#    define LIBCYPHAL_INTROSPECTION_TRACE_ENABLE 0
#endif

/// Enable runtime asserts within libcyphal.
/// Obviously, this is not a recommended thing to turn on in production code but
/// these asserts are used for some library tests and may help diagnose bugs if
/// enabled in a special build of your software.
#ifndef LIBCYPHAL_INTROSPECTION_ENABLE_ASSERT
#    define LIBCYPHAL_INTROSPECTION_ENABLE_ASSERT 0
#endif

#if LIBCYPHAL_INTROSPECTION_TRACE_ENABLE

#    include <cstdio>

#    define LIBCYPHAL_TRACEF(src, fmt, ...) std::printf("%s: " fmt "\r\n", src, __VA_ARGS__)
#    define LIBCYPHAL_TRACE(src, msg) std::printf("%s: " msg "\r\n", src)

#else

#    define LIBCYPHAL_TRACEF(...) ((void) 0)
#    define LIBCYPHAL_TRACE(...) ((void) 0)

#endif

#endif  // LIBCYPHAL_INTROSPECTION_HPP_INCLUDED
