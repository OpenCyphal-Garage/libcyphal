#
# Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
# Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#

cmake_minimum_required(VERSION 3.5.1)

project(libuavcan CXX)

set(LIBUAVCAN_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/libuavcan/include")

# +---------------------------------------------------------------------------+
# | EXTERNAL DEPENDENCIES
# +---------------------------------------------------------------------------+
#
# Program : virtualenv
#
# We require virtual env to allow python to run naturally but without polluting
# the host system.
#
find_program(VIRTUALENV virtualenv REQUIRED)

set(VIRTUALENV_OUTPUT ${CMAKE_BINARY_DIR}/python_venv)

execute_process(COMMAND ${VIRTUALENV} -p python3 ${VIRTUALENV_OUTPUT}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

set(PYTHON ${VIRTUALENV_OUTPUT}/bin/python3)
set(PYTHON_REQUIREMENTS ${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt)
set(PIP ${VIRTUALENV_OUTPUT}/bin/pip3)
set(PYTEST ${VIRTUALENV_OUTPUT}/bin/py.test)

#
# Pypi: pull python dependencies from PyPi
#
# Pull packages we need to support our build and test environment.
#
execute_process(COMMAND ${PIP} --disable-pip-version-check --isolated install -r ${PYTHON_REQUIREMENTS}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})


#
# Framework : Googletest
#
# (Taken from googletest/README.md documentation)
# GTest executables
# Download and unpack googletest at configure time. We don't
# model this as a submodule because this technique properly utilizes
# the googletest cmake build.
#
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
# Git submodule management.
#
# (Taken from gitlab's ["modern CMake"](https://cliutils.gitlab.io/modern-cmake/))
#
# It would probably be better to model each of these submodules as cmake external projects
# (like we do for googletest) but for now they are just git submodules.
#
find_package(Git QUIET)

if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
#   Update submodules as needed
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/submodules/pydsdl/README.md")
    message(FATAL_ERROR "The pydsdl submodule was not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/submodules/dsdl/README.md")
    message(FATAL_ERROR "The dsdl submodule was not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()

# +---------------------------------------------------------------------------+
# | SOURCE GENERATION
# +---------------------------------------------------------------------------+
#
# DSDLC compiler invocation
#
set(DSDLC_PYDSDL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/submodules/pydsdl")
set(DSDLC_INPUTS "${CMAKE_CURRENT_SOURCE_DIR}/submodules/dsdl/uavcan")
set(DSDLC_OUTPUT "${CMAKE_BINARY_DIR}/dsdlc_generated")
set(DSDLC_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/commons/dsdl_compiler/libuavcan_dsdlc")
set(DSDLC_TEMPLATE "${LIBUAVCAN_INCLUDE}/uavcan/data_type_template.tmpl")
set(DSDLC_INPUT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/libuavcan")

foreach(DSDLC_INPUT ${DSDLC_INPUTS})
    file(GLOB_RECURSE DSDLC_NEW_INPUT_FILES ${CMAKE_CURRENT_SOURCE_DIR} "${DSDLC_INPUT}/*.uavcan")
    set(DSDLC_INPUT_FILES ${DSDLC_INPUT_FILES} ${DSDLC_NEW_INPUT_FILES})
endforeach(DSDLC_INPUT)

execute_process(COMMAND ${PYTHON} ${DSDLC_SCRIPT} --pydsdl-path ${DSDLC_PYDSDL_PATH} ${DSDLC_INPUTS} --dry-run
                OUTPUT_VARIABLE DSDLC_OUTPUT_FILES
                RESULT_VARIABLE DSDLC_DRY_RUN_RESULT)

if(NOT DSDLC_DRY_RUN_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to retrieve a list of headers the dsdlc would generate (${DSDLC_DRY_RUN_RESULT})")
endif()

if("${DSDLC_OUTPUT_FILES}" STREQUAL "")
    message(FATAL_ERROR "No header files would be generated for DSDL input \"${DSDLC_INPUTS}\"")
endif()

add_custom_command(OUTPUT ${DSDLC_OUTPUT_FILES}
                   COMMAND ${PYTHON} ${DSDLC_SCRIPT} --pydsdl-path ${DSDLC_PYDSDL_PATH} -T ${DSDLC_TEMPLATE} ${DSDLC_INPUTS} -O${DSDLC_OUTPUT} -v
                   DEPENDS ${DSDLC_INPUT_FILES} ${DSDLC_TEMPLATE}
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   COMMENT "Running dsdl compiler")

add_custom_target(libuavcan_dsdlc DEPENDS ${DSDLC_OUTPUT_FILES})
include_directories(${DSDLC_OUTPUT})

# +---------------------------------------------------------------------------+
# | BUILD AND RUN NATIVE UNIT TESTS
# +---------------------------------------------------------------------------+
#
# Set flags
#
include_directories(
    ${LIBUAVCAN_INCLUDE}
    ${CMAKE_BINARY_DIR}/dsdl_generated
    ${DSDLC_OUTPUT}
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
file(GLOB_RECURSE TEST_CXX_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "test/native/*.cpp")
add_executable(libuavcan_test_native ${TEST_CXX_FILES})
add_dependencies(libuavcan_test_native libuavcan_dsdlc)
target_link_libraries(libuavcan_test_native gmock_main)

# Tests run automatically upon successful build
# If failing tests need to be investigated with debugger, use 'make --ignore-errors'
add_test(NAME libuavcan_test_native
         COMMAND libuavcan_test_native
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
