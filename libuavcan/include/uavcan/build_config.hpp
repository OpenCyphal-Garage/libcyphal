/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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
 * UAVCAN_CPP_VERSION - version of the C++ standard used during compilation.
 * This definition contains the integer year number after which the standard was named:
 *  - 2011 for C++11
 *  - 2014 for C++14
 *  - 2017 for C++17
 *
 * This config automatically sets according to the actual C++ standard used by the compiler.
 *
 * libuavcan v2.0 relies on the standard library and requires features introduced in c++11.
 * While the library does not require standards newer then c++11 it does compile correctly 
 * in code that compiles using the newer 2014 and 2017 standards.
 */
#define UAVCAN_CPP11    2011
#define UAVCAN_CPP14    2014
#define UAVCAN_CPP17    2017

#ifndef UAVCAN_CPP_VERSION
# if (__cplusplus >= 201800L)
#  error Unknown C++ standard. You can explicitly set UAVCAN_CPP_VERSION=UAVCAN_CPP17 to silence this error.
# elif (__cplusplus > 201700L)
#  define UAVCAN_CPP_VERSION    UAVCAN_CPP17
# elif (__cplusplus > 201400L)
#  define UAVCAN_CPP_VERSION    UAVCAN_CPP14
# elif (__cplusplus > 201100L)
#  define UAVCAN_CPP_VERSION    UAVCAN_CPP11
# else
#  error Unsupported C++ standard (C++11 or greater required). You can explicitly set UAVCAN_CPP_VERSION=UAVCAN_CPP11 to silence this error.
# endif
#endif

#endif // UAVCAN_BUILD_CONFIG_HPP_INCLUDED
