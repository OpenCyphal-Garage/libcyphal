#
# Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
# Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#

cmake_minimum_required(VERSION 3.6.0)

project(libuavcan ASM CXX C)

if (NOT DEFINED LIBUAVCAN_PROJECT_ROOT)
    get_filename_component(LIBUAVCAN_PROJECT_ROOT
                           "${CMAKE_SOURCE_DIR}/../"
                           REALPATH BASE_DIR
                           "${CMAKE_BINARY_DIR}")
    message(STATUS "Setting LIBUAVCAN_PROJECT_ROOT = ${LIBUAVCAN_PROJECT_ROOT}")
else()
    message(STATUS "Using ${LIBUAVCAN_PROJECT_ROOT} for LIBUAVCAN_PROJECT_ROOT")
endif()

include(${CMAKE_SOURCE_DIR}/cmake/common.cmake)

#
# Make sure we have our external project directory created. We use this
# to share external project artifacts with different build flavors.
#
file(MAKE_DIRECTORY ${EXTERNAL_PROJECT_DIRECTORY})

set(LIBUAVCAN_INCLUDE "${LIBUAVCAN_PROJECT_ROOT}/libuavcan/include")
set(LIBUAVCAN_INCLUDE_LVS "${LIBUAVCAN_PROJECT_ROOT}/libuavcan_validation_suite/include")

include_directories(
    ${LIBUAVCAN_INCLUDE}
    ${LIBUAVCAN_INCLUDE_LVS}
)

if(NOT LIBUAVCAN_FLAG_SET)
    message(STATUS "LIBUAVCAN_FLAG_SET was not defined. Setting to \"native\" as a default")
    set(LIBUAVCAN_FLAG_SET ${CMAKE_SOURCE_DIR}/cmake/compiler_flag_sets/native.cmake)
else()
    set(LIBUAVCAN_FLAG_SET ${CMAKE_BINARY_DIR}/${LIBUAVCAN_FLAG_SET})
endif()

if(NOT LIBUAVCAN_TESTBUILD)
    message(STATUS "LIBUAVCAN_TESTBUILD was not defined. Setting to \"native\" as a default")
    set(LIBUAVCAN_TESTBUILD ${CMAKE_SOURCE_DIR}/native/tests.cmake)
else()
    set(LIBUAVCAN_TESTBUILD ${CMAKE_BINARY_DIR}/${LIBUAVCAN_TESTBUILD})
endif()

# +---------------------------------------------------------------------------+
# | EXTERNAL DEPENDENCIES
# +---------------------------------------------------------------------------+
#
# Tell cmake where to find our custom scripts.
#
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

#
# We prefer to install python dependencies for the build in an virtualenv.
# If you don't have this setup you'll need to manually make any python
# dependencies available to the version of python the build will use.
#
find_package(virtualenv)

#
# DSDL public, regulated types for validating libuavcan.
#
find_package(public_regulated_data_types REQUIRED)

#
# googletest (and googlemock) external project.
#
find_package(gtest REQUIRED)

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
find_package(nnvg REQUIRED)

create_dsdl_target(dsdl-regulated
                   ${CMAKE_BINARY_DIR}/public-regulated-data-types-gen
                   ${LIBUAVCAN_PROJECT_ROOT}/libuavcan/include/libuavcan/templates
                   ${EXTERNAL_PROJECT_DIRECTORY}/public-regulated-data-types-src/uavcan
                   ON)

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
create_check_style_target(format-check ON "${LIBUAVCAN_INCLUDE}/**/*.hpp")

# +---------------------------------------------------------------------------+
# | BUILD UNIT TESTS
# +---------------------------------------------------------------------------+
include(${LIBUAVCAN_TESTBUILD})

# +---------------------------------------------------------------------------+
# | DEFINE COMPILE-TIME TESTS
# +---------------------------------------------------------------------------+
include(${CMAKE_SOURCE_DIR}/compile/compile_tests.cmake)

# +---------------------------------------------------------------------------+
# | DOCUMENTATION GENERATION
# +---------------------------------------------------------------------------+
if(NOT DEFINED LIBUAVCAN_SKIP_DOCS OR NOT LIBUAVCAN_SKIP_DOCS)
    create_docs_target(docs OFF)
else()
    message(STATUS "LIBUAVCAN_SKIP_DOCS is ON. Skipping documentation generation for this build.")
endif()
