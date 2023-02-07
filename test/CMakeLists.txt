#
# Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
# Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#

cmake_minimum_required(VERSION 3.16.0)

project(libcyphal ASM CXX C)

if (NOT DEFINED LIBCYPHAL_PROJECT_ROOT)
    get_filename_component(LIBCYPHAL_PROJECT_ROOT
                           "${CMAKE_SOURCE_DIR}/../"
                           REALPATH BASE_DIR
                           "${CMAKE_BINARY_DIR}")
    message(STATUS "Setting LIBCYPHAL_PROJECT_ROOT = ${LIBCYPHAL_PROJECT_ROOT}")
else()
    message(STATUS "Using ${LIBCYPHAL_PROJECT_ROOT} for LIBCYPHAL_PROJECT_ROOT")
endif()

include(${CMAKE_SOURCE_DIR}/cmake/common.cmake)

#
# Make sure we have our external project directory created. We use this
# to share external project artifacts with different build flavors.
#
file(MAKE_DIRECTORY ${EXTERNAL_PROJECT_DIRECTORY})

set(LIBCYPHAL_INCLUDE "${LIBCYPHAL_PROJECT_ROOT}/libcyphal/include")
set(LIBCYPHAL_INCLUDE_LVS "${LIBCYPHAL_PROJECT_ROOT}/libcyphal_validation_suite/include")

include_directories(
    ${LIBCYPHAL_INCLUDE}
    ${LIBCYPHAL_INCLUDE_LVS}
)

if(NOT LIBCYPHAL_FLAG_SET)
    message(STATUS "LIBCYPHAL_FLAG_SET was not defined. Setting to \"native\" as a default")
    set(LIBCYPHAL_FLAG_SET ${CMAKE_SOURCE_DIR}/cmake/compiler_flag_sets/native.cmake)
else()
    set(LIBCYPHAL_FLAG_SET ${CMAKE_BINARY_DIR}/${LIBCYPHAL_FLAG_SET})
endif()

if(NOT LIBCYPHAL_TESTBUILD)
    message(STATUS "LIBCYPHAL_TESTBUILD was not defined. Setting to \"native\" as a default")
    set(LIBCYPHAL_TESTBUILD ${CMAKE_SOURCE_DIR}/native/tests.cmake)
else()
    set(LIBCYPHAL_TESTBUILD ${CMAKE_BINARY_DIR}/${LIBCYPHAL_TESTBUILD})
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
# DSDL public, regulated types for validating libcyphal.
#
find_package(public_regulated_data_types REQUIRED)

#
# googletest (and googlemock) external project.
#
find_package(gtest REQUIRED)

#
# Finds programs needed to build the libcyphal documentation
#
find_package(libcyphal_docs REQUIRED)

#
# We require clang-format to check the style as part of CI builds.
#
find_package(clangformat REQUIRED)

# +---------------------------------------------------------------------------+
# | SOURCE GENERATION
# +---------------------------------------------------------------------------+
find_package(nnvg REQUIRED)

create_dsdl_target(dsdl-support
                   cpp
                   cpp-14
                   ${CMAKE_BINARY_DIR}/public-regulated-data-types-gen
                   ${EXTERNAL_PROJECT_DIRECTORY}/public-regulated-data-types-src/uavcan
                   OFF
                   ON
                   OFF
                   ON
                   ON
                   "any"
                   "only"
)

create_dsdl_target(dsdl-regulated
                   cpp
                   cpp-14
                   ${CMAKE_BINARY_DIR}/public-regulated-data-types-gen
                   ${EXTERNAL_PROJECT_DIRECTORY}/public-regulated-data-types-src/uavcan
                   OFF
                   ON
                   OFF
                   ON
                   ON
                   "any"
                   "never"
)


add_dependencies(dsdl-regulated dsdl-support)

# +---------------------------------------------------------------------------+
# | FLAG SETS
# +---------------------------------------------------------------------------+
#
# Load all our compile flag sets into the appropriate places.
#
apply_flag_set(${LIBCYPHAL_FLAG_SET})

# +---------------------------------------------------------------------------+
# | STYLE
# +---------------------------------------------------------------------------+
create_check_style_target(format-check ON "${LIBCYPHAL_INCLUDE}/**/*.hpp")

# +---------------------------------------------------------------------------+
# | BUILD UNIT TESTS
# +---------------------------------------------------------------------------+
include(${LIBCYPHAL_TESTBUILD})

# +---------------------------------------------------------------------------+
# | DEFINE COMPILE-TIME TESTS
# +---------------------------------------------------------------------------+
include(${CMAKE_SOURCE_DIR}/compile/compile_tests.cmake)

# +---------------------------------------------------------------------------+
# | DOCUMENTATION GENERATION
# +---------------------------------------------------------------------------+
if(NOT DEFINED LIBCYPHAL_SKIP_DOCS OR NOT LIBCYPHAL_SKIP_DOCS)
    create_docs_target(docs OFF)
else()
    message(STATUS "LIBCYPHAL_SKIP_DOCS is ON. Skipping documentation generation for this build.")
endif()
