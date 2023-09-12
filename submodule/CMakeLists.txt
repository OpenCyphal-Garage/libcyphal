#
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.16.3)

project(libcyphal_test_external C CXX)

include("../CMakeCommon.cmake" REQUIRED)

# +---------------------------------------------------------------------------+
# | CMAKE EXTERNAL PROJECT CRAP
# |    See ${CMAKE_SOURCE_DIR}/cmake/functions/update_external_projects.cmake
# +---------------------------------------------------------------------------+

add_custom_target(update_cmake_external_projects
                  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/functions/update_external_projects.cmake)

# +---------------------------------------------------------------------------+
# | EXTERNAL LIBRARIES
# +---------------------------------------------------------------------------+
# Build libcanard.
add_project_library(
    NAME canard
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/libcanard/libcanard/canard.c
    HEADER_PATH
        ${CMAKE_CURRENT_SOURCE_DIR}/libcanard/libcanard/
    HEADER_INCLUDE_PATTERNS
        *.h
    STATIC
    FPIC
)

# Build libudpard.
add_project_library(
    NAME udpard
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/libudpard/libudpard/udpard.c
    HEADER_PATH
        ${CMAKE_CURRENT_SOURCE_DIR}/libudpard/libudpard/
    HEADER_INCLUDE_PATTERNS
        *.h
    STATIC
    FPIC
)

# Build o1heap.
add_project_library(
    NAME o1heap
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/o1heap/o1heap/o1heap.c
    HEADER_PATH
        ${CMAKE_CURRENT_SOURCE_DIR}/o1heap/o1heap/
    HEADER_INCLUDE_PATTERNS
        *.h
    STATIC
    FPIC
)

# Build CETL.
add_project_library(
    NAME cetl
    HEADER_PATH
        ${CMAKE_CURRENT_SOURCE_DIR}/cetl/include/
)
