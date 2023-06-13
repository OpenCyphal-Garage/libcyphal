#
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.22.0)

project(libcyphal_posix CXX)

include("../../cmake/CMakeCommon.cmake" REQUIRED)

include(ProjectLibrary)
find_package(cyphal REQUIRED)
find_package(cetl REQUIRED)
find_package(public_regulated_data_types REQUIRED)
find_package(nnvg REQUIRED)

set(TARGET_INSTALL_DIR "include")

# +---------------------------------------------------------------------------+
# | BUILD DEMONSTRATION TYPES
# +---------------------------------------------------------------------------+
if (CETL_ENABLE_DEBUG_ASSERT)
    set(LOCAL_NNVG_ASSERT_ARGS "ENABLE_SER_ASSERT")
else()
    set(LOCAL_NNVG_ASSERT_ARGS )
endif()

if (${CMAKE_MESSAGE_LOG_LEVEL} MATCHES "VERBOSE|TRACE|DEBUG")
    set(LOCAL_NNVG_VERBOSE_ARGS "VERBOSE")
else()
    set(LOCAL_NNVG_VERBOSE_ARGS )
endif()

define_nuanvut_support_target(
    TARGET posix_dsdl_support
    OUTPUT_FOLDER ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${LOCAL_NNVG_ASSERT_ARGS}
    ${LOCAL_NNVG_VERBOSE_ARGS}
)

add_dsdl_cpp_codegen(
    TARGET posix_dsdl_public_types
    OUTPUT_FOLDER ${CMAKE_CURRENT_BINARY_DIR}/generated
    DSDL_ROOT_DIR ${public_regulated_data_types_SOURCE_DIR}/uavcan
    ${LOCAL_NNVG_ASSERT_ARGS}
    ${LOCAL_NNVG_VERBOSE_ARGS}
    DSDL_DEPENDENCIES
        posix_dsdl_support
)

# +---------------------------------------------------------------------------+
# | cyphal_posix library
# +---------------------------------------------------------------------------+

add_executable(
    libcyphal_demo_posix
    src/main.cpp
    src/context.cpp
    src/sockets.cpp
    src/pollers.cpp
)

set(LOCAL_MAP_FILE "${CMAKE_CURRENT_BINARY_DIR}/libcyphal_demo_posix.map")

# TODO: apple clang doesn't like this. Figure out why.
#target_link_options(libcyphal_demo_posix PUBLIC LINKER:-Map=${LOCAL_MAP_FILE})
set_target_properties(libcyphal_demo_posix PROPERTIES
                      ADDITIONAL_CLEAN_FILES ${LOCAL_MAP_FILE})
add_dependencies(libcyphal_demo_posix posix_dsdl_public_types)
target_link_libraries(libcyphal_demo_posix
                      cyphal
)
target_include_directories(libcyphal_demo_posix
                           PUBLIC
                            ${CMAKE_CURRENT_BINARY_DIR}/generated
                            ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(libcyphal_demo_posix PUBLIC NUNAVUT_ASSERT=assert)
# +---------------------------------------------------------------------------+