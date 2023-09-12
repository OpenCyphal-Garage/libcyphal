# Copyright Amazon.com Inc., or its affiliates. All Rights Reserved.

cmake_minimum_required(VERSION 3.16.3)

project(libcyphal_package CXX)

include(../CMakeCommon.cmake REQUIRED)

# +---------------------------------------------------------------------------+
# | (lib)cyphal
# +---------------------------------------------------------------------------+
add_project_library(
    NAME cyphal
    HEADER_PATH
        ${CMAKE_CURRENT_SOURCE_DIR}/include/
    LIBRARIES
        canard
        udpard
)

# +---------------------------------------------------------------------------+
