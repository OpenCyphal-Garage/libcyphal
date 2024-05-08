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
find_package(public_regulated_data_types REQUIRED)
find_package(nnvg REQUIRED)
find_package(googletest REQUIRED)
if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
find_package(gcovr REQUIRED)
endif()

# +---------------------------------------------------------------------------+
# | BUILD EXAMPLE TYPES
# +---------------------------------------------------------------------------+
if (CETL_ENABLE_DEBUG_ASSERT)
    set(NNVG_ASSERT_ARGS "ENABLE_SER_ASSERT")
else()
    set(NNVG_ASSERT_ARGS )
endif()

if (${CMAKE_MESSAGE_LOG_LEVEL} MATCHES "VERBOSE|TRACE|DEBUG")
    set(NNVG_VERBOSE_ARGS "VERBOSE")
else()
    set(NNVG_VERBOSE_ARGS )
endif()

define_nuanvut_support_target(
    TARGET dsdl_support
    ${NNVG_ASSERT_ARGS}
    ${NNVG_VERBOSE_ARGS}
)

add_dsdl_cpp_codegen(
    TARGET dsdl_public_types
    DSDL_ROOT_DIR ${public_regulated_data_types_SOURCE_DIR}/uavcan
    ${NNVG_ASSERT_ARGS}
    ${NNVG_VERBOSE_ARGS}
    DSDL_DEPENDENCIES
        dsdl_support
)

add_dsdl_cpp_codegen(
    TARGET dsdl_example_types
    DSDL_ROOT_DIR dsdl/example
    ${NNVG_ASSERT_ARGS}
    ${NNVG_VERBOSE_ARGS}
    DSDL_DEPENDENCIES
        dsdl_support
        dsdl_public_types
)

# +---------------------------------------------------------------------------+
# | BUILD EXAMPLE CODE
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
        EXTRA_TEST_LIBS cyphal cetl dsdl_example_types
        LINK_TO_MAIN
        OUT_TEST_LIB_VARIABLE LOCAL_TEST_LIB
        OUT_TEST_EXE_VARIABLE LOCAL_TEST_TARGET
        OUT_TEST_REPORT_VARIABLE LOCAL_TEST_REPORT
    )
    if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
        define_gcovr_tracefile_target(
            TARGET ${LOCAL_TEST_TARGET}
            ROOT_DIRECTORY ${LIBCYPHAL_ROOT}
            TARGET_EXECUTION_DEPENDS ${LOCAL_TEST_REPORT}
            OBJECT_LIBRARY ${LOCAL_TEST_LIB}
            EXCLUDE_TEST_FRAMEWORKS
            EXCLUDE_TARGET
            ENABLE_INSTRUMENTATION
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
    enable_coverage_report(COVERAGE_REPORT_FORMATS html
                           ROOT_DIRECTORY ${LIBCYPHAL_ROOT}
    )
endif()
