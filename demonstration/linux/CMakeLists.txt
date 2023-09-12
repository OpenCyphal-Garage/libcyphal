#
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.16.3)

project(libcyphal_posix CXX)

include("../../CMakeCommon.cmake" REQUIRED)

set(TARGET_INSTALL_DIR "include")

add_definitions(-DLIBCYPHAL_TRANSPORT_MAX_FIFO_QUEUE_SIZE=358)
add_definitions(-DLIBCYPHAL_TRANSPORT_MAX_BROADCASTS=50)
add_definitions(-DLIBCYPHAL_TRANSPORT_MAX_SUBSCRIPTIONS=50)
add_definitions(-DLIBCYPHAL_TRANSPORT_MAX_MESSAGE_SIZE_BYTES=501200)
# Max Heap size is currently set to Max Message Size * 2
add_definitions(-DLIBCYPHAL_TRANSPORT_MAX_HEAP_SIZE=1002400)

# +---------------------------------------------------------------------------+
# | cyphal_posix library
# +---------------------------------------------------------------------------+

add_project_library(
    NAME cyphal_posix
    SOURCES
        src/storage_can.cpp
        src/storage_udp.cpp
    HEADER_PATH
        include/
    LIBRARIES
        o1heap
        cyphal
    STATIC
)

add_executable(
    libcyphal_demo_posix
    src/main.cpp
)
target_link_libraries(libcyphal_demo_posix cyphal_posix cyphal)

add_executable(
    libcyphal_demo_posix_node_42
    src/example_node_42.cpp
)
target_link_libraries(libcyphal_demo_posix_node_42 cyphal_posix cyphal)

add_executable(
    libcyphal_demo_posix_node_43
    src/example_node_43.cpp
)
target_link_libraries(libcyphal_demo_posix_node_43 cyphal_posix cyphal)

# +---------------------------------------------------------------------------+