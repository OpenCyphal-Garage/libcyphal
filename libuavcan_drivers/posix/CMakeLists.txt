#
# Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
# Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#

cmake_minimum_required(VERSION 2.8)

project(libuavcan_posix)

if(NOT DEFINED DEBUG_BUILD)
    message(ERROR, "DEBUG_BUILD hasn't been defined. This must be either 1 or 0")
endif()

#
# Library (header only)
#
install(DIRECTORY include/uavcan_posix DESTINATION include)

#
# Finding libuavcan - it will be a target if we're running from the top-level CMakeLists.txt,
# otherwise fail since we don't accidentally want to use an incorrect version.
#
if (TARGET uavcan)
    message(STATUS "Using uavcan target; source dir: ${libuavcan_SOURCE_DIR}")
    set(UAVCAN_LIB uavcan)
    include_directories(${libuavcan_SOURCE_DIR}/include
                        ${libuavcan_SOURCE_DIR}/include/dsdlc_generated)
    message(STATUS "POSIX source dir: ${libuavcan_posix_SOURCE_DIR}")
    include_directories(${libuavcan_posix_SOURCE_DIR}/include)
else ()
    message(ERROR "uavcan library was not found.")
endif ()

#
# Tests
#
include_directories(include)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -std=c++11")

if(DEBUG_BUILD)
    add_definitions(-DUAVCAN_DEBUG=1)
endif()

#
# Common unit-test executable setup.
#
function(add_driver_test name library)

    file(GLOB_RECURSE TEST_CXX_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "test/*.cpp")
    add_executable(${name} ${TEST_CXX_FILES})
    add_dependencies(${name} ${library})

    #Tests should always be built as debug
    set_target_properties(${name} PROPERTIES COMPILE_FLAGS "-DUAVCAN_DEBUG=1")

    target_link_libraries(${name} gmock_main)
    target_link_libraries(${name} ${library})

    add_test(NAME ${name}
             COMMAND ${name}
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

endfunction()

# GTest executables
if (GTEST_FOUND)
    message(STATUS "GTest found, tests will be built and run.")
    add_driver_test(test_posixdriver uavcan)
else (GTEST_FOUND)
    message(STATUS "GTest was not found, tests will not be built")
endif (GTEST_FOUND)
        