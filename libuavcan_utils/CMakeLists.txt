#
# Copyright (C) 2015 Pavel Kirienko <pavel.kirienko@gmail.com>
# Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#

cmake_minimum_required(VERSION 2.8)

project(libuavcan_posix_utils)


if(NOT DEFINED POSIX_PLATFORM)
    message(ERROR, "POSIX_PLATFORM hasn't been defined. This variable is required.")
endif()
#
# includes
#
install(DIRECTORY posix/include/uavcan_posix DESTINATION include)
install(DIRECTORY ${POSIX_PLATFORM}/include/uavcan_${POSIX_PLATFORM} DESTINATION include)

include_directories(posix/include
                    ${libuavcan_posix_SOURCE_DIR}/include
                    ${POSIX_PLATFORM}/include)
#
# System dependecies 
#
find_package(Threads REQUIRED)

#
# Finding libuavcan - it will be a target if we're running from the top-level CMakeLists.txt,
# otherwise try to find it in the system directories.
#
if (TARGET uavcan)
    message(STATUS "Using uavcan target; source dir: ${libuavcan_SOURCE_DIR}")
    set(UAVCAN_LIB uavcan)
    include_directories(${libuavcan_SOURCE_DIR}/include
                        ${libuavcan_SOURCE_DIR}/include/dsdlc_generated)
    message(STATUS "POSIX source dir: ${libuavcan_posix_SOURCE_DIR}")
else ()
    message(ERROR "uavcan library was not found.")
endif ()

#
# Applications - tests, tools.
#
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -std=c++11")  # GCC or Clang

if(DEBUG_BUILD)
    add_definitions(-DUAVCAN_DEBUG=1)
endif()

#
# add_libuavcan_driver_util - Adds executable and a standard set of libraries to build utilities.
#
function(add_libuavcan_driver_util name source)
    add_executable(${name} ${source})
    if(${POSIX_PLATFORM} STREQUAL "linux")
        target_link_libraries(${name} rt)
    endif()
    target_link_libraries(${name} ${UAVCAN_LIB} ${CMAKE_THREAD_LIBS_INIT})
    install(TARGETS ${name} RUNTIME DESTINATION bin)
endfunction()

#
# Target verification, any platform.
#
add_libuavcan_driver_util(test_node posix/apps/test_node.cpp)

#
# Utilities, any platform
#
add_libuavcan_driver_util(uavcan_nodetool posix/apps/uavcan_nodetool.cpp)

if(${POSIX_PLATFORM} STREQUAL "linux")
    #
    # Target verification, linux-only
    #
    add_libuavcan_driver_util(test_clock linux/apps/test_clock.cpp)
    add_libuavcan_driver_util(test_socket linux/apps/test_socket.cpp)
    add_libuavcan_driver_util(test_time_sync linux/apps/test_time_sync.cpp)
    add_libuavcan_driver_util(test_system_utils linux/apps/test_system_utils.cpp)
    add_libuavcan_driver_util(test_posix linux/apps/test_posix.cpp)
    add_libuavcan_driver_util(test_dynamic_node_id_client linux/apps/test_dynamic_node_id_client.cpp)
    add_libuavcan_driver_util(test_file_server linux/apps/test_file_server.cpp)
    add_libuavcan_driver_util(test_multithreading linux/apps/test_multithreading.cpp)
    #
    # Utilities, linux-only
    #
    add_libuavcan_driver_util(uavcan_monitor linux/apps/uavcan_monitor.cpp)
    add_libuavcan_driver_util(uavcan_dynamic_node_id_server linux/apps/uavcan_dynamic_node_id_server.cpp)

else()
    message(ERROR "${POSIX_PLATFORM} is not a supported posix platform for this build.")
endif()

