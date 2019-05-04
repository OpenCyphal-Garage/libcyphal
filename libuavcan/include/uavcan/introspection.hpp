/*
 * Optional utilites to provide introspection into libuavcan
 * objects and to provide metrics for library performance.
 * Facilities defined in this header can be compiled out of
 * production code and should be used for debugging or targeted
 * testing activities.
 *
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#ifndef UAVCAN_INTROSPECTION_HPP_INCLUDED
#define UAVCAN_INTROSPECTION_HPP_INCLUDED

#include "uavcan/uavcan.hpp"

#ifndef UAVCAN_INTROSPECTION_TRACE_ENABLE
#    define UAVCAN_INTROSPECTION_TRACE_ENABLE 0
#endif

#if UAVCAN_INTROSPECTION_TRACE_ENABLE

#    if __GNUC__
__attribute__((format(printf, 2, 3)))
#    endif
static void
UAVCAN_TRACE(const char* src, const char* fmt, ...)
{
    va_list args;
    (void) std::printf("UAVCAN: %s: ", src);
    va_start(args, fmt);
    (void) std::vprintf(fmt, args);
    va_end(args);
    (void) std::puts("");
}

#else

#    define UAVCAN_TRACE(...) ((void) 0)

#endif

#if UAVCAN_ENABLE_ASSERT
#    include <cassert>
#    define UAVCAN_ASSERT(statement) \
        {                            \
            assert(statement);       \
        }
#else
#    define UAVCAN_ASSERT(...) ((void) 0)
#endif

#endif  // UAVCAN_INTROSPECTION_HPP_INCLUDED
