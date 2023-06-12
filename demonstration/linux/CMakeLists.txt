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

set(TARGET_INSTALL_DIR "include")

# +---------------------------------------------------------------------------+
# | cyphal_posix library
# +---------------------------------------------------------------------------+

add_project_library(
    NAME cyphal_posix
    HEADER_PATH
        include/
    LIBRARIES
        cetl
        cyphal
)

add_executable(
    libcyphal_demo_posix
    src/main.cpp
)

set(LOCAL_MAP_FILE "${CMAKE_CURRENT_BINARY_DIR}/libcyphal_demo_posix.map")

target_link_options(libcyphal_demo_posix PUBLIC LINKER:-Map=${LOCAL_MAP_FILE})
set_target_properties(libcyphal_demo_posix PROPERTIES
                      ADDITIONAL_CLEAN_FILES ${LOCAL_MAP_FILE})
target_link_libraries(libcyphal_demo_posix cyphal_posix)

# +---------------------------------------------------------------------------+