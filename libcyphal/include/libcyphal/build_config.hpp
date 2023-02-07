/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/**
 * @file
 * Build-time configuration macros, templates, and constant expressions.
 */

#ifndef LIBCYPHAL_BUILD_CONFIG_HPP_INCLUDED
#define LIBCYPHAL_BUILD_CONFIG_HPP_INCLUDED

// +--------------------------------------------------------------------------+
/** @defgroup macros_versioning Versioning macros.
 *
 * Macros specifying and controlling versions of the library, the Cyphal
 * specification, compilers, standards, etc.
 *  @{
 */

#define LIBCYPHAL_VERSION_MAJOR 2 /**< libcyphal major version definition. */
#define LIBCYPHAL_VERSION_MINOR 0 /**< libcyphal minor version definition. */

/**
 * The Cyphal specification version this version of libcyphal adheres to.
 */
#define LIBCYPHAL_CYPHAL_SPECIFICATION_VERSION_MAJOR 1

/**
 * libcyphal C++ version check - validate the version of the C++ standard used during compilation.
 *
 * libcyphal v2.0 makes careful use of the standard library and requires features introduced
 * in c++11. While the library does not require standards newer then c++11 it does compile
 * correctly in code that compiles using the newer 2014 and 2017 standards.
 *
 * Also note that libcyphal functions normally without enabling c++ exceptions and makes no
 * use of RTTI. It furthermore does not use the standard heap for allocations on any platform.
 *
 * You can define LIBCYPHAL_CPP_VERSION_NOCHECK to skip these checks.
 */

#ifndef LIBCYPHAL_CPP_VERSION_NOCHECK
#    if (__cplusplus >= 201800L)
#        error Unknown C++ standard. You can explicitly define LIBCYPHAL_CPP_VERSION_NOCHECK to silence this error.
#    elif (__cplusplus >= 201402L)
#        define LIBCYPHAL_CPP_VERSION_NOCHECK 1
#    else
#  error Unsupported C++ standard (C++14 or greater required). You can explicitly set LIBCYPHAL_CPP_VERSION_NOCHECK to silence this error.
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
#ifndef LIBCYPHAL_EXPORT
#    define LIBCYPHAL_EXPORT
#endif

/**
 * C++ feature test macros are newer then c++11 so we translate them into a "LIBCYPHAL"
 * macro to allow us to manually enable exceptions if the compiler doesn't define the __cpp
 * macro.
 */
#ifdef __cpp_exceptions
#    ifndef LIBCYPHAL_ENABLE_EXCEPTIONS
#        define LIBCYPHAL_ENABLE_EXCEPTIONS 1
#    endif
#endif

/** @} */  // end of macros_platform

#endif  // LIBCYPHAL_BUILD_CONFIG_HPP_INCLUDED
