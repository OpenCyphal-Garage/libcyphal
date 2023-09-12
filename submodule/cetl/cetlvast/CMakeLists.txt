#
# Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.22.0)

###############################################################################
# DON'T USE CMAKE!
#
#     ...directly that is. We provide a verify.py script that wraps up a bunch
# of conventions and environment variables that you'd otherwise need to know.
# Use it to invoke cmake or at least use it to tell you how to invoke cmake.
# We also provide full vscode cmake integration that has the same knowledge
# of cetlvast that the verify.py script has.
###############################################################################

# +---------------------------------------------------------------------------+
# | PROJECT
# +---------------------------------------------------------------------------+

project(cetlvast CXX)

if (NOT DEFINED CETL_ROOT)
    set(LOCAL_PARENT_OF_SOURCE_PATH "${CMAKE_SOURCE_DIR}/../")
    cmake_path(ABSOLUTE_PATH LOCAL_PARENT_OF_SOURCE_PATH
               OUTPUT_VARIABLE CETL_ROOT
               BASE_DIRECTORY "${CMAKE_BINARY_DIR}")
    cmake_path(NORMAL_PATH CETL_ROOT OUTPUT_VARIABLE CETL_ROOT)
    message(DEBUG "Setting CETL_ROOT = ${CETL_ROOT}")
else()
    message(DEBUG "Using ${CETL_ROOT} for CETL_ROOT")
endif()


if (NOT DEFINED CETLVAST_PROJECT_ROOT)
    set(LOCAL_PARENT_OF_SOURCE_PATH "${CMAKE_SOURCE_DIR}/../")
    cmake_path(ABSOLUTE_PATH CMAKE_SOURCE_DIR
               OUTPUT_VARIABLE CETLVAST_PROJECT_ROOT
               BASE_DIRECTORY "${CMAKE_BINARY_DIR}")
    cmake_path(NORMAL_PATH CMAKE_BINARY_DIR OUTPUT_VARIABLE CMAKE_BINARY_DIR)
    message(DEBUG "Setting CETLVAST_PROJECT_ROOT = ${CETLVAST_PROJECT_ROOT}")
else()
    message(DEBUG "Using ${CETLVAST_PROJECT_ROOT} for CETLVAST_PROJECT_ROOT")
endif()

if (NOT DEFINED CETLVAST_EXT_FOLDER)
    cmake_path(GET CMAKE_CURRENT_BINARY_DIR FILENAME CETLVAST_BUILD_DIR_STUB)
    set(EXTERNAL_PROJECT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${CETLVAST_BUILD_DIR_STUB}_ext")
else()
    set(EXTERNAL_PROJECT_DIRECTORY "${CETLVAST_EXT_FOLDER}")
endif()

message(DEBUG "Ensuring EXTERNAL_PROJECT_DIRECTORY=${EXTERNAL_PROJECT_DIRECTORY}")

#
# Make sure we have our external project directory created. We use this
# to share external project artifacts with different build flavors.
#
file(MAKE_DIRECTORY ${EXTERNAL_PROJECT_DIRECTORY})

cmake_path(APPEND CETL_ROOT "include" OUTPUT_VARIABLE CETL_INCLUDE)

include_directories(
    ${CETL_INCLUDE}
    "${CETLVAST_PROJECT_ROOT}/include"
)

if(DEFINED CETLVAST_CMAKE_MODULE_PATH)
    message(DEBUG "CETLVAST_CMAKE_MODULE_PATH was set to ${CETLVAST_CMAKE_MODULE_PATH}. Will use this to override CMAKE_MODULE_PATH.")
    set(CMAKE_MODULE_PATH ${CETLVAST_CMAKE_MODULE_PATH})
else()
    #
    # Tell cmake where to find our custom scripts.
    #
    set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
    message(DEBUG "CMAKE_MODULE_PATH set to ${CMAKE_MODULE_PATH}")
endif()


message(STATUS
"[ globals ]-----------------------------------------------\n\
    CETL_ROOT:                  ${CETL_ROOT}\n\
    CETL_INCLUDE:               ${CETL_INCLUDE}\n\
    CMAKE_MODULE_PATH:          ${CMAKE_MODULE_PATH}\n\
    CETLVAST_PROJECT_ROOT:      ${CETLVAST_PROJECT_ROOT}\n\
    EXTERNAL_PROJECT_DIRECTORY: ${EXTERNAL_PROJECT_DIRECTORY}\n\
    CMAKE_SOURCE_DIR:           ${CMAKE_SOURCE_DIR}\n\
    CMAKE_CURRENT_BINARY_DIR:   ${CMAKE_CURRENT_BINARY_DIR}\n\
    CMAKE_MESSAGE_LOG_LEVEL:    ${CMAKE_MESSAGE_LOG_LEVEL}\n\
-----------------------------------------------------------\n\
")

# +---------------------------------------------------------------------------+
# | OPTIONS/VARIANTS
# +---------------------------------------------------------------------------+

option(CETL_ENABLE_DEBUG_ASSERT "Enable or disable runtime asserts in CETL code." OFF)
option(CETLVAST_ENABLE_COVERAGE "Enable or disable coverage data instrumentation in test builds." OFF)
option(CETLVAST_STYLE_CHECK "Enable or disable a check for violations of the project's style standards." ON)

set(CETLVAST_CPP_STANDARD "20" CACHE STRING "C++ standard to use when compiling.")
set_property(CACHE CETLVAST_CPP_STANDARD PROPERTY STRINGS 14 17 20)

set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type.")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Release Debug)

set(CETLVAST_COVERAGE_REPORT_FORMAT "html" CACHE STRING "Coverage reporting standard to use when building coverage reports.")
set_property(CACHE CETLVAST_COVERAGE_REPORT_FORMAT PROPERTY STRINGS "html" "sonarqube" "json")

message(STATUS "[ Build Options ]--------------------------------------\n\
    CMAKE_BUILD_TYPE:                ${CMAKE_BUILD_TYPE}\n\
    CETL_ENABLE_DEBUG_ASSERT:        ${CETL_ENABLE_DEBUG_ASSERT}\n\
    CETLVAST_ENABLE_COVERAGE:        ${CETLVAST_ENABLE_COVERAGE}\n\
    CETLVAST_COVERAGE_REPORT_FORMAT: ${CETLVAST_COVERAGE_REPORT_FORMAT}\n\
    CETLVAST_STYLE_CHECK:            ${CETLVAST_STYLE_CHECK}\n\
    CETLVAST_CPP_STANDARD:           ${CETLVAST_CPP_STANDARD}\n\
-----------------------------------------------------------\n\
")

# Always need this.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# +---------------------------------------------------------------------------+
# | KITS
# +---------------------------------------------------------------------------+

if(NOT DEFINED CETLVAST_FLAG_SET)
    message(VERBOSE "CETLVAST_FLAG_SET was not defined. Setting to \"native\" as a default")
    set(CETLVAST_FLAG_SET ${CMAKE_SOURCE_DIR}/cmake/compiler_flag_sets/native.cmake)
else()
    set(CETLVAST_FLAG_SET ${CETLVAST_FLAG_SET})
endif()

if(NOT DEFINED CETLVAST_TEST_SUITE)
    message(VERBOSE "CETLVAST_TEST_SUITE was not defined. Setting to \"unittest\" as a default")
    set(CETLVAST_TEST_SUITE ${CMAKE_SOURCE_DIR}/cmake/suites/unittest.cmake)
else()
    set(CETLVAST_TEST_SUITE ${CETLVAST_TEST_SUITE})
endif()

message(STATUS "[ Build Kit ]-------------------------------------------\n\
    CMAKE_TOOLCHAIN_FILE:     ${CMAKE_TOOLCHAIN_FILE}\n\
    CETLVAST_FLAG_SET:        ${CETLVAST_FLAG_SET}\n\
    CETLVAST_TEST_SUITE:      ${CETLVAST_TEST_SUITE}\n\
-----------------------------------------------------------\n\
")

# +---------------------------------------------------------------------------+
# | FLAG SETS
# +---------------------------------------------------------------------------+
#
# Load all our compile flag sets into the appropriate places.
#
include(${CETLVAST_FLAG_SET})

# +---------------------------------------------------------------------------+
# | BUILD SELECTED TEST SUITE
# +---------------------------------------------------------------------------+
include(${CETLVAST_TEST_SUITE})
