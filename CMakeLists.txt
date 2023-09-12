#
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.16.3)

project(libcyphal CXX)

# until we can fix our stupid build system it's better to have less bugs
# than more performance.
set(CMAKE_BUILD_TYPE debug CACHE STRING "" FORCE)

include(CMakeCommon.cmake REQUIRED)

# +---------------------------------------------------------------------------+
# | SUBPROJECTS
# +---------------------------------------------------------------------------+
add_subdirectory(libcyphal)
add_subdirectory(submodule)
add_subdirectory(demonstration/linux)

# +---------------------------------------------------------------------------+
# | DEFAULT BUILD SUPPORT
# +---------------------------------------------------------------------------+
add_custom_target(
    release 
    DEPENDS
        cyphal
)

add_custom_target(
    coverage
    DEPENDS
        cyphal   
)

add_custom_target(
    doc
    DEPENDS
        cyphal   
)

add_custom_target(
    test
    DEPENDS
        cyphal
)
