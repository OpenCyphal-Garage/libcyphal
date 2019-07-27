/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/**
 * @file
 * Build-time configuration macros, templates, and constant expressions.
 */

#ifndef LIBUAVCAN_BUILD_CONFIG_HPP_INCLUDED
#define LIBUAVCAN_BUILD_CONFIG_HPP_INCLUDED

// +--------------------------------------------------------------------------+
/** @defgroup macros_versioning Versioning macros.
 *
 * Macros specifying and controlling versions of the library, the UAVCAN
 * specification, compilers, standards, etc.
 *  @{
 */

#define LIBUAVCAN_VERSION_MAJOR 2 /**< libuavcan major version definition. */
#define LIBUAVCAN_VERSION_MINOR 0 /**< libuavcan minor version definition. */

/**
 * The UAVCAN specification version this version of libuavcan adheres to.
 */
#define LIBUAVCAN_UAVCAN_SPECIFICATION_VERSION_MAJOR 1

/**
 * LibUAVCAN C++ version check - validate the version of the C++ standard used during compilation.
 *
 * libuavcan v2.0 makes careful use of the standard library and requires features introduced
 * in c++11. While the library does not require standards newer then c++11 it does compile
 * correctly in code that compiles using the newer 2014 and 2017 standards.
 *
 * Also note that libuavcan functions normally without enabling c++ exceptions and makes no
 * use of RTTI. It furthermore does not use the standard heap for allocations on any platform.
 *
 * You can define LIBUAVCAN_CPP_VERSION_NOCHECK to skip these checks.
 */

#ifndef LIBUAVCAN_CPP_VERSION_NOCHECK
#    if (__cplusplus >= 201800L)
#        error Unknown C++ standard. You can explicitly define LIBUAVCAN_CPP_VERSION_NOCHECK to silence this error.
#    elif (__cplusplus > 201100L)
#        define LIBUAVCAN_CPP_VERSION_NOCHECK 1
#    else
#  error Unsupported C++ standard (C++11 or greater required). You can explicitly set LIBUAVCAN_CPP_VERSION_NOCHECK to silence this error.
#    endif
#endif

/** @} */  // end of macros_versioning

// +--------------------------------------------------------------------------+
/** @defgroup macros_platform Platform integration macros.
 *
 * Macros that allow the library to be customized for different
 * target platforms.
 *  @{
 */
/**
 * Declaration visibility
 * http://gcc.gnu.org/wiki/Visibility
 */
#ifndef LIBUAVCAN_EXPORT
#    define LIBUAVCAN_EXPORT
#endif

/**
 * C++ feature test macros are newer then c++11 so we translate them into a "LIBUAVCAN"
 * macro to allow us to manually enable exceptions if the compiler doesn't define the __cpp
 * macro.
 */
#ifdef __cpp_exceptions
#    ifndef LIBUAVCAN_ENABLE_EXCEPTIONS
#        define LIBUAVCAN_ENABLE_EXCEPTIONS 1
#    endif
#endif

/** @} */  // end of macros_platform

#endif  // LIBUAVCAN_BUILD_CONFIG_HPP_INCLUDED
