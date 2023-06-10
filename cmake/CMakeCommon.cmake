#
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.22.0)

# +---------------------------------------------------------------------------+
# | LIBCYPHAL SUBPROJECT COMMON
# +---------------------------------------------------------------------------+

if(NOT DEFINED CMAKE_MESSAGE_LOG_LEVEL)
    set(CMAKE_MESSAGE_LOG_LEVEL STATUS)
endif()

if (NOT DEFINED LIBCYPHAL_VERSION)
    if (NOT DEFINED CYPHAL_PROJECT_VERSION)
        message(WARNING "Neither LIBCYPHAL_VERSION nor CYPHAL_PROJECT_VERSION was not set. Setting LIBCYPHAL_VERSION to default of 0.0.0.")
        set(LIBCYPHAL_VERSION "0.0.0")
    else()
        set(LIBCYPHAL_VERSION ${CYPHAL_PROJECT_VERSION})
    endif()
endif()

add_compile_definitions("LIBCYPHAL_VERSION=\"${LIBCYPHAL_VERSION}\"")


# Uses CMAKE_CURRENT_FUNCTION_LIST_DIR variable to set LIBCYPHAL_ROOT
# in the caller's scope as a absolute path to the folder this
# (cmake/CMakeCommon.cmake) file is in.
function (set_libcyphal_project_root_from_common)
    cmake_path(ABSOLUTE_PATH CMAKE_CURRENT_FUNCTION_LIST_DIR
               OUTPUT_VARIABLE LIBCYPHAL_ROOT
               BASE_DIRECTORY "${CMAKE_SOURCE_DIR}")
    cmake_path(APPEND LIBCYPHAL_ROOT ".." OUTPUT_VARIABLE LIBCYPHAL_ROOT)
    cmake_path(NORMAL_PATH LIBCYPHAL_ROOT OUTPUT_VARIABLE LIBCYPHAL_ROOT)
    set(LIBCYPHAL_ROOT ${LIBCYPHAL_ROOT} PARENT_SCOPE)
endfunction(set_libcyphal_project_root_from_common)

if (NOT DEFINED LIBCYPHAL_ROOT)
    set_libcyphal_project_root_from_common()
    message(DEBUG "Setting LIBCYPHAL_ROOT = ${LIBCYPHAL_ROOT}")
else()
    message(DEBUG "Using ${LIBCYPHAL_ROOT} for LIBCYPHAL_ROOT")
endif()

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    message(DEBUG "CMAKE_INSTALL_PREFIX was a default. Setting it to CMAKE_BINARY_DIR (${CMAKE_BINARY_DIR}) to avoid installing in the local system.")
    set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR} CACHE PATH "The folder under which all install destinations will be placed." FORCE)
endif()

if(NOT DEFINED CETLVAST_FLAG_SET)
    message(VERBOSE "CETLVAST_FLAG_SET was not defined. Setting to \"default\" as a default")
    cmake_path(APPEND LIBCYPHAL_ROOT "cmake" "compiler_flag_sets" "default.cmake" OUTPUT_VARIABLE CETLVAST_FLAG_SET)
endif()

set(CETLVAST_CPP_STANDARD "14" CACHE STRING "C++ standard to use when compiling.")
set_property(CACHE CETLVAST_CPP_STANDARD PROPERTY STRINGS 14 17 20)

set(CMAKE_CXX_STANDARD ${CETLVAST_CPP_STANDARD})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (CETL_ENABLE_DEBUG_ASSERT AND NOT LIBCYPHAL_INTROSPECTION_ENABLE_ASSERT)
    message(STATUS "CETL_ENABLE_DEBUG_ASSERT was defined so we're enabling libcyphal introspection.")
    set(LIBCYPHAL_INTROSPECTION_ENABLE_ASSERT 1)
endif()

if (LIBCYPHAL_INTROSPECTION_TRACE_ENABLE)
    add_compile_definitions("LIBCYPHAL_INTROSPECTION_TRACE_ENABLE=1")
else()
    set(LIBCYPHAL_INTROSPECTION_TRACE_ENABLE 0)
endif()

if (LIBCYPHAL_INTROSPECTION_ENABLE_ASSERT)
    add_compile_definitions("LIBCYPHAL_INTROSPECTION_ENABLE_ASSERT=1")
else()
    set(LIBCYPHAL_INTROSPECTION_ENABLE_ASSERT 0)
endif()

if (LIBCXX_ENABLE_ASSERTIONS)
    add_compile_definitions("LIBCXX_ENABLE_ASSERTIONS=1")
else()
    set(LIBCXX_ENABLE_ASSERTIONS 0)
endif()

cmake_path(APPEND LIBCYPHAL_ROOT "cmake" "modules" OUTPUT_VARIABLE LIBCYPHAL_CMAKE_MODULE_PATH)
if(NOT LIBCYPHAL_CMAKE_MODULE_PATH IN_LIST CMAKE_MODULE_PATH)
    list(APPEND CMAKE_MODULE_PATH ${LIBCYPHAL_CMAKE_MODULE_PATH})
endif()

# +---------------------------------------------------------------------------+
# | CETL and CETLVAST
# +---------------------------------------------------------------------------+
# Libcyphal borrows cmake modules from cetl/cetlvast. This section makes sure
# to setup a few globals that suite needs to have right before including it.

cmake_path(APPEND LIBCYPHAL_ROOT "external" OUTPUT_VARIABLE CETLVAST_EXTERNAL_ROOT)

if (CETL_ENABLE_DEBUG_ASSERT)
    add_compile_definitions("CETL_ENABLE_DEBUG_ASSERT=1")
else()
    set(CETL_ENABLE_DEBUG_ASSERT 0)
endif()

find_package(cetl REQUIRED)


# +---------------------------------------------------------------------------+
# | FLAG SETS
# +---------------------------------------------------------------------------+
#
# Load all our compile flag sets into the appropriate places.
#
include(${CETLVAST_FLAG_SET})


# +---------------------------------------------------------------------------+
# | CUSTOM PROPERTIES
# +---------------------------------------------------------------------------+
define_property(TARGET
    PROPERTY TEST_FRAMEWORK_LINK_LIBRARIES
    BRIEF_DOCS "Libraries in a target that are part of the test framework."
    FULL_DOCS "Libraries in a target that are part of the test framework rather than being the test subject itself. "
              "The set of TEST_FRAMEWORK_LINK_LIBRARIES subtracted from the target's LINK_LIBRARIES is the set of "
              "libraries that are part of the test subject."
)

define_property(TARGET
    PROPERTY POST_BUILD_INSTRUMENTATION_BYPRODUCTS
    BRIEF_DOCS "Files produced by the target when used."
    FULL_DOCS "Files produced by the target in a post-build step, or a custom command, due to instrumentation "
              "like coverage or sanitizers. These files are not part of the target's normal build artifacts."
)

define_property(DIRECTORY
    PROPERTY IN_BUILD_TESTS INHERITED
    BRIEF_DOCS "A set of targets in the folder that should be run as part of the build."
    FULL_DOCS "A set of targets in the folder that should be run as part of the build. This allows builds to "
              "fail if tests fail and prevents invalid artifacts from being produced."
)


# +---------------------------------------------------------------------------+
# | LIBCYPHAL
# +---------------------------------------------------------------------------+
find_package(cyphal REQUIRED)

