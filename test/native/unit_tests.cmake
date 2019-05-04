#
# Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#

#
# Test : All C++ Unit Tests
#
file(GLOB_RECURSE TEST_CXX_FILES 
     RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} 
     "test/native/*.cpp")

add_executable(${PROJECT_NAME} ${TEST_CXX_FILES})
add_dependencies(${PROJECT_NAME} dsdl-regulated)

target_link_libraries(${PROJECT_NAME} gmock_main)

add_test(NAME ${PROJECT_NAME}
         COMMAND ${PROJECT_NAME}
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
