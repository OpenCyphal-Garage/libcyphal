#
# Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
# Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#

cmake_minimum_required(VERSION 3.6.0)

project(libuavcan ASM CXX C)

include(${CMAKE_SOURCE_DIR}/cmake/common.cmake)

#
# Make sure we have our external project directory created. We use this
# to share external project artifacts with different build flavors.
#
file(MAKE_DIRECTORY ${EXTERNAL_PROJECT_DIRECTORY})

set(LIBUAVCAN_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/libuavcan/include")
set(LIBUAVCAN_INCLUDE_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/dsdl")

include_directories(
    ${LIBUAVCAN_INCLUDE}
    ${LIBUAVCAN_INCLUDE_GENERATED}
)

if(NOT LIBUAVCAN_FLAG_SET)
    message(STATUS "LIBUAVCAN_FLAG_SET was not defined. Setting to \"native\" as a default")
    set(LIBUAVCAN_FLAG_SET ${CMAKE_CURRENT_BINARY_DIR}/../cmake/compiler_flag_sets/native.cmake)
else()
    set(LIBUAVCAN_FLAG_SET ${CMAKE_CURRENT_BINARY_DIR}/${LIBUAVCAN_FLAG_SET})
endif()

if(NOT LIBUAVCAN_TESTBUILD)
    message(STATUS "LIBUAVCAN_TESTBUILD was not defined. Setting to \"native\" as a default")
    set(LIBUAVCAN_TESTBUILD ${CMAKE_CURRENT_BINARY_DIR}/../test/native/unit_tests.cmake)
else()
    set(LIBUAVCAN_TESTBUILD ${CMAKE_CURRENT_BINARY_DIR}/${LIBUAVCAN_TESTBUILD})
endif()

# +---------------------------------------------------------------------------+
# | EXTERNAL DEPENDENCIES
# +---------------------------------------------------------------------------+
#
# Tell cmake where to find our custom scripts.
#
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

#
# DSDL public, regulated types for validating libuavcan.
#
find_package(public_regulated_data_types REQUIRED)

#
# googletest (and googlemock) external project.
#
find_package(gtest REQUIRED)

#
# We generate coverage reports. Please look at them.
#
find_package(lcov REQUIRED)

#
# Finds programs needed to build the libuavcan documentation
#
find_package(libuavcan_docs REQUIRED)

#
# We require clang-format to check the style as part of CI builds.
#
find_package(clangformat REQUIRED)

# +---------------------------------------------------------------------------+
# | SOURCE GENERATION
# +---------------------------------------------------------------------------+
find_package(dsdlgenj REQUIRED)

create_dsdl_target(dsdl-regulated
                   OFF
                   ${EXTERNAL_PROJECT_DIRECTORY}/public-regulated-data-types-gen
                   ${CMAKE_CURRENT_SOURCE_DIR}/libuavcan/include/uavcan/templates
                   ${EXTERNAL_PROJECT_DIRECTORY}/public-regulated-data-types-src/uavcan)

# +---------------------------------------------------------------------------+
# | FLAG SETS
# +---------------------------------------------------------------------------+
#
# Load all our compile flag sets into the appropriate places.
#
apply_flag_set(${LIBUAVCAN_FLAG_SET})

# +---------------------------------------------------------------------------+
# | STYLE
# +---------------------------------------------------------------------------+
create_apply_style_target(format-generated OFF ${dsdl-regulated-OUTPUT})

add_dependencies(format-generated dsdl-regulated)

create_check_style_target(format-check ON "${LIBUAVCAN_INCLUDE}/**/*.hpp")

# +---------------------------------------------------------------------------+
# | BUILD AND RUN UNIT TESTS
# +---------------------------------------------------------------------------+
include(${LIBUAVCAN_TESTBUILD})

# +---------------------------------------------------------------------------+
# | DOCUMENTATION GENERATION
# +---------------------------------------------------------------------------+
if(NOT DEFINED LIBUAVCAN_SKIP_DOCS OR NOT LIBUAVCAN_SKIP_DOCS)
    create_docs_target(docs OFF)
else()
    message(STATUS "LIBUAVCAN_SKIP_DOCS is ON. Skipping documentation generation for this build.")
endif()
