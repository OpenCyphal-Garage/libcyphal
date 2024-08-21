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
# | BUILD EXAMPLE TYPES
# +---------------------------------------------------------------------------+

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

if(UNIX AND NOT (CMAKE_SYSTEM_NAME STREQUAL "Windows"))
    set(IS_POSIX TRUE)
else ()
    set(IS_POSIX FALSE)
endif()
if(UNIX AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(IS_LINUX TRUE)
else ()
    set(IS_LINUX FALSE)
endif()
message(STATUS "IS_POSIX=${IS_POSIX}")
message(STATUS "IS_LINUX=${IS_LINUX}")

set(EXAMPLES_PLATFORM_LIBS "")

if(IS_POSIX)
    add_library(examples_platform_posix
        "platform/posix/udp/udp.c"
    )
    list(APPEND EXAMPLES_PLATFORM_LIBS "examples_platform_posix")
endif()
if(IS_LINUX)
    add_library(examples_platform_linux
        "platform/linux/can/socketcan.c"
    )
    target_link_libraries(examples_platform_linux PUBLIC canard)
    list(APPEND EXAMPLES_PLATFORM_LIBS "examples_platform_linux")
endif()
message(STATUS "EXAMPLES_PLATFORM_LIBS=${EXAMPLES_PLATFORM_LIBS}")

file(GLOB NATIVE_EXAMPLES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        example_*.cpp
)

set(ALL_EXAMPLES "")
set(ALL_EXAMPLE_RUNS "")

foreach(NATIVE_EXAMPLE ${NATIVE_EXAMPLES})

    message(STATUS "Example: ${NATIVE_EXAMPLE}")

    # Skip linux specific examples on non-linux platforms
    #
    string(FIND "${NATIVE_EXAMPLE}" "linux" LINUX_WORD_POSITION)
    if((LINUX_WORD_POSITION GREATER -1) AND NOT IS_LINUX)
        message(WARNING "skipping linux specific example: ${NATIVE_EXAMPLE}")
        continue()
    endif()

    define_native_gtest_unittest_targets(
        TEST_SOURCE ${NATIVE_EXAMPLE}
        EXTRA_TEST_LIBS cyphal cetl dsdl_support dsdl_example_types ${EXAMPLES_PLATFORM_LIBS}
        LINK_TO_MAIN
        OUT_TEST_LIB_VARIABLE LOCAL_TEST_LIB
        OUT_TEST_EXE_VARIABLE LOCAL_TEST_TARGET
        OUT_TEST_REPORT_VARIABLE LOCAL_TEST_REPORT
    )

    # We need to exclude the "external", "nunavut" & "uavcan" directories from coverage reports.
    cmake_path(APPEND LIBCYPHAL_ROOT "external" OUTPUT_VARIABLE LIBCYPHAL_EXTERNAL_PATH)
    cmake_path(APPEND LIBCYPHAL_ROOT "${CMAKE_INSTALL_PREFIX}/nunavut" OUTPUT_VARIABLE LIBCYPHAL_NUNAVUT_PATH)
    cmake_path(APPEND LIBCYPHAL_ROOT "${CMAKE_INSTALL_PREFIX}/uavcan" OUTPUT_VARIABLE LIBCYPHAL_UAVCAN_PATH)

    if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
        define_gcovr_tracefile_target(
            TARGET ${LOCAL_TEST_TARGET}
            ROOT_DIRECTORY ${LIBCYPHAL_ROOT}
            TARGET_EXECUTION_DEPENDS ${LOCAL_TEST_REPORT}
            OBJECT_LIBRARY ${LOCAL_TEST_LIB}
            EXCLUDE_PATHS ${LIBCYPHAL_EXTERNAL_PATH} ${LIBCYPHAL_NUNAVUT_PATH} ${LIBCYPHAL_UAVCAN_PATH}
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
    enable_coverage_report(COVERAGE_REPORT_FORMATS html sonarqube
                           ROOT_DIRECTORY ${LIBCYPHAL_ROOT}
    )
endif()
