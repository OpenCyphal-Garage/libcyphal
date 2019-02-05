/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#ifndef UAVCAN_BUILD_CONFIG_HPP_INCLUDED
#define UAVCAN_BUILD_CONFIG_HPP_INCLUDED

// +--------------------------------------------------------------------------+
// | VERSIONING
// |    This section contains configuration for versions of the library itself,
// | compilers, c++ standards, etc.
// +--------------------------------------------------------------------------+
/**
 * libuavcan version definition
 */
#define UAVCAN_LIBRARY_VERSION_MAJOR 2
#define UAVCAN_LIBRARY_VERSION_MINOR 0

/**
 * UAVCAN specification version this implementation
 * adheres to.
 */
#define UAVCAN_SPECIFICATION_VERSION_MAJOR 1

/**
 * UAVCAN C++ version check - validate the version of the C++ standard used during compilation.
 *
 * libuavcan v2.0 makes careful use of the standard library and requires features introduced
 * in c++11. While the library does not require standards newer then c++11 it does compile
 * correctly in code that compiles using the newer 2014 and 2017 standards.
 *
 * Also note that libuavcan functions normally without enabling c++ exceptions and makes no
 * use of RTTI. It furthermore does not use the standard heap for allocations on any platform.
 *
 * You can define UAVCAN_CPP_VERSION_NOCHECK to skip these checks.
 */

#ifndef UAVCAN_CPP_VERSION_NOCHECK
#    if (__cplusplus >= 201800L)
#        error Unknown C++ standard. You can explicitly define UAVCAN_CPP_VERSION_NOCHECK to silence this error.
#    elif (__cplusplus > 201100L)
#        define UAVCAN_CPP_VERSION_NOCHECK
#    else
#  error Unsupported C++ standard (C++11 or greater required). You can explicitly set UAVCAN_CPP_VERSION_NOCHECK to silence this error.
#    endif
#endif

// +--------------------------------------------------------------------------+
// | PLATFORM HOOKS
// |    This section contains macros that allow a platform to customize how
// | the library is built or how it behaves.
// +--------------------------------------------------------------------------+
/**
 * Declaration visibility
 * http://gcc.gnu.org/wiki/Visibility
 */
#ifndef UAVCAN_EXPORT
#    define UAVCAN_EXPORT
#endif

// +--------------------------------------------------------------------------+
// | CAPABILITIES
// |    This section contains macros to configure what capabilites to enable
// | in the library where the capabilites may change the amount of memory
// | or CPU required at runtime.
// +--------------------------------------------------------------------------+
/**
 * Enable support for CAN-FD.
 *
 * This feature will cause some buffers to be larger to accommodate the larger
 * frame size of CAN-FD. The larger payload size translates into larger message
 * sizes which may impact performance where message payloads must be transferred
 * through system memory.
 */
#ifndef UAVCAN_ENABLE_FD
#    define UAVCAN_ENABLE_FD 1
#endif

/**
 * Enable runtime asserts within libuavcan.
 *
 * Obviously, this is not a reccommended thing to turn on in production code but
 * these asserts are used for some library tests and may help diagnose bugs if
 * enabled in a special build of your software.
 */
#ifndef UAVCAN_ENABLE_ASSERT
#    define UAVCAN_ENABLE_ASSERT 0
#endif

#endif  // UAVCAN_BUILD_CONFIG_HPP_INCLUDED
