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
 * UAVCAN version definition
 */
#define UAVCAN_VERSION_MAJOR    2
#define UAVCAN_VERSION_MINOR    0

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
# if (__cplusplus >= 201800L)
#  error Unknown C++ standard. You can explicitly define UAVCAN_CPP_VERSION_NOCHECK to silence this error.
# elif (__cplusplus > 201100L)
#  define UAVCAN_CPP_VERSION_NOCHECK
# else
#  error Unsupported C++ standard (C++11 or greater required). You can explicitly set UAVCAN_CPP_VERSION_NOCHECK to silence this error.
# endif
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
# define UAVCAN_EXPORT
#endif

#endif // UAVCAN_BUILD_CONFIG_HPP_INCLUDED
