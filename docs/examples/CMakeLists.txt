#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.22.0)

# +---------------------------------------------------------------------------+
# | PROJECT
# +---------------------------------------------------------------------------+

project(libcyphal_docs_examples C CXX)

include("../../cmake/CMakeCommon.cmake" REQUIRED)

find_package(cyphal REQUIRED)
find_package(googletest REQUIRED)
if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
find_package(gcovr REQUIRED)
endif()

# +---------------------------------------------------------------------------+
# | BUILD NATIVE EXAMPLE CODE
# +---------------------------------------------------------------------------+

file(GLOB NATIVE_EXAMPLES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        example_*.cpp
)

set(ALL_EXAMPLES "")
set(ALL_EXAMPLE_RUNS "")

foreach(NATIVE_EXAMPLE ${NATIVE_EXAMPLES})
    define_native_gtest_unittest_targets(
        TEST_SOURCE ${NATIVE_EXAMPLE}
        EXTRA_TEST_LIBS cyphal
        OUT_TEST_LIB_VARIABLE LOCAL_TEST_LIB
        OUT_TEST_EXE_VARIABLE LOCAL_TEST_TARGET
        OUT_TEST_REPORT_VARIABLE LOCAL_TEST_REPORT
    )
    if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
        define_gcovr_tracefile_target(
            TARGET ${LOCAL_TEST_TARGET}
            TARGET_EXECUTION_DEPENDS ${LOCAL_TEST_REPORT}
            EXCLUDE_TEST_FRAMEWORKS
            EXCLUDE_TARGET
            OUT_TRACEFILE_VARIABLE LOCAL_TEST_TRACEFILE
        )
    endif()
    list(APPEND ALL_EXAMPLES "${LOCAL_TEST_TARGET}")
    list(APPEND ALL_EXAMPLE_RUNS "${LOCAL_TEST_REPORT}")
endforeach()

add_custom_target(
     build_examples
     DEPENDS
          ${ALL_EXAMPLES}
)

add_custom_target(
     run_examples
     DEPENDS
          ${ALL_EXAMPLE_RUNS}
)

set_directory_properties(PROPERTIES
    IN_BUILD_TESTS "${ALL_EXAMPLE_RUNS}"
)

if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
    enable_html_report()
endif()