#
# Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
# Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#

cmake_minimum_required(VERSION 3.5.1)

project(libuavcan CXX)

#
# Framework : Googletest
#
# (Taken from googletest/README.md documentation)
# GTest executables
# Download and unpack googletest at configure time
configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
if(result)
    message(WARNING "CMake step for googletest failed: ${result}")
else()
    execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
    if(result)
        message(WARNING "Build step for googletest failed: ${result}")
    else()

        # Prevent overriding the parent project's compiler/linker
        # settings on Windows
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

        # Add googletest directly to our build. This defines
        # the gtest and gtest_main targets.
        add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src
                        ${CMAKE_BINARY_DIR}/googletest-build
                        EXCLUDE_FROM_ALL)

        set(GTEST_FOUND ON)
        set(BUILD_TESTING ON)
        enable_testing()
    endif()
endif()

#
# Set flags
#
include_directories(
    ./libuavcan/include/
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti -fno-exceptions -fno-threadsafe-statics")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Og -DNDEBUG -ggdb -fno-strict-aliasing")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -pedantic -Wfloat-equal -Wconversion -Wabi")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunused-parameter -Wunused-variable -Wunused-value")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wcast-align -Wmissing-declarations -Wno-missing-field-initializers ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdouble-promotion -Wswitch-enum -Wtype-limits -Wno-error=array-bounds")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wzero-as-null-pointer-constant -Wnon-virtual-dtor")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Woverloaded-virtual -Wsign-promo -Wold-style-cast")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsign-conversion")

#
# Test : All Unit Tests
#
file(GLOB_RECURSE TEST_CXX_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "test/*.cpp")
add_executable(libuavcan_test_native ${TEST_CXX_FILES})

target_link_libraries(libuavcan_test_native gmock_main)

# Tests run automatically upon successful build
# If failing tests need to be investigated with debugger, use 'make --ignore-errors'
add_test(NAME libuavcan_test_native
         COMMAND libuavcan_test_native
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
