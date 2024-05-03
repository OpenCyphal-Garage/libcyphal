#
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.22.0)

include(cmake/CMakeCommon.cmake REQUIRED)

project(libcyphal
        VERSION ${LIBCYPHAL_VERSION}
        LANGUAGES CXX C
        HOMEPAGE_URL https://github.com/OpenCyphal-Garage/libcyphal
)

# Use -DNO_STATIC_ANALYSIS=1 to suppress static analysis.
# If not suppressed, the tools used here shall be available, otherwise the build will fail.
if (NOT NO_STATIC_ANALYSIS)
    # clang-tidy (separate config files per directory)
    find_program(clang_tidy NAMES clang-tidy)
    if (NOT clang_tidy)
        message(FATAL_ERROR "Could not locate clang-tidy")
    endif ()
    message(STATUS "Using clang-tidy: ${clang_tidy}")
endif ()

add_subdirectory(test/unittest)
add_subdirectory(docs)

add_custom_target(
    docs
    DEPENDS
        generate_libcyphal_docs
)

add_custom_target(
    build
    DEPENDS
        build_unittests
        build_examples
)

if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
    add_custom_target(
        unittest
        DEPENDS
            run_unittests
            gcovr_html_report_for_unittest
            run_examples
            gcovr_html_report_for_examples
    )
else()
    add_custom_target(
        unittest
        DEPENDS
            run_unittests
            run_examples
    )
endif()

add_custom_target(
    release
    DEPENDS
        build
        unittest
        docs
)

if (${CMAKE_CXX_PLATFORM_ID} STREQUAL "Linux")
    message(STATUS "TODO: add GNU/Linux demos")
    #add_subdirectory(demonstration/linux)
    #add_dependencies(build libcyphal_demo_posix)
endif()

# Don't normalize deviance: if CMAKE_TOOLCHAIN_FILE is not set then provide
# an initialized default to display in the status thus avoiding a warning about
# using an uninitialized variable.
if (NOT CMAKE_TOOLCHAIN_FILE)
    set(LOCAL_CMAKE_TOOLCHAIN_FILE "(not set)")
else()
    set(LOCAL_CMAKE_TOOLCHAIN_FILE ${CMAKE_TOOLCHAIN_FILE})
endif()

message(STATUS "-------------------------------------------------------------------\n\
| ${CMAKE_PROJECT_NAME}
-- -------------------------------------------------------------------\n\
    LIBCYPHAL_ROOT:                         ${LIBCYPHAL_ROOT}\n\
    LIBCYPHAL_INCLUDE:                      ${LIBCYPHAL_INCLUDE}\n\
    LIBCYPHAL_VERSION:                      ${LIBCYPHAL_VERSION}\n\
    LIBCYPHAL_INTROSPECTION_TRACE_ENABLE:   ${LIBCYPHAL_INTROSPECTION_TRACE_ENABLE}\n\
    LIBCYPHAL_INTROSPECTION_ENABLE_ASSERT:  ${LIBCYPHAL_INTROSPECTION_ENABLE_ASSERT}\n\
    CETLVAST_CPP_STANDARD:                  ${CETLVAST_CPP_STANDARD}\n\
    CMAKE_PROJECT_NAME:                     ${CMAKE_PROJECT_NAME}\n\
    CMAKE_SOURCE_DIR:                       ${CMAKE_SOURCE_DIR}\n\
    CMAKE_CURRENT_SOURCE_DIR:               ${CMAKE_CURRENT_SOURCE_DIR}\n\
    CMAKE_BINARY_DIR:                       ${CMAKE_BINARY_DIR}\n\
    CMAKE_CURRENT_BINARY_DIR:               ${CMAKE_CURRENT_BINARY_DIR}\n\
    CMAKE_MODULE_PATH:                      ${CMAKE_MODULE_PATH}\n\
    CMAKE_MESSAGE_LOG_LEVEL:                ${CMAKE_MESSAGE_LOG_LEVEL}\n\
    CMAKE_INSTALL_PREFIX:                   ${CMAKE_INSTALL_PREFIX}\n\
    CMAKE_BUILD_TYPE:                       ${CMAKE_BUILD_TYPE}\n\
    FETCHCONTENT_FULLY_DISCONNECTED:        ${FETCHCONTENT_FULLY_DISCONNECTED}\n\
\n\
CETL
    CETL_ENABLE_DEBUG_ASSERT:               ${CETL_ENABLE_DEBUG_ASSERT}\n\
\n\
Standard Library\n\
    LIBCXX_ENABLE_ASSERTIONS:               ${LIBCXX_ENABLE_ASSERTIONS}\n\
 -- ------------------------------------------------------------------\n\
")

message(STATUS "[ Build Kit ]-------------------------------------------\n\
    CMAKE_TOOLCHAIN_FILE:           ${LOCAL_CMAKE_TOOLCHAIN_FILE}\n\
    CETLVAST_FLAG_SET:              ${CETLVAST_FLAG_SET}\n\
    CMAKE_CXX_COMPILER_[ID-VER]:    ${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}\n\
    CMAKE_C_COMPILER_[ID-VER]:      ${CMAKE_CXX_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}\n\
    CMAKE_AR:                       ${CMAKE_AR}\n\
    CMAKE_LINKER:                   ${CMAKE_LINKER}\n\
    CMAKE_RANLIB:                   ${CMAKE_RANLIB}\n\
    CMAKE_[lang]_PLATFORM_ID:       ${CMAKE_CXX_PLATFORM_ID}\n\
-----------------------------------------------------------\n\
")
