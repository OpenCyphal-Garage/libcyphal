#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

include(FetchContent)
set(cetl_GIT_REPOSITORY "https://github.com/OpenCyphal/cetl.git")
set(cetl_GIT_TAG "db5062a500fd537ebb01e0babb13f2adaeac105f")

FetchContent_Declare(
    cetl
    GIT_REPOSITORY  ${cetl_GIT_REPOSITORY}
    GIT_TAG         ${cetl_GIT_TAG}
    SOURCE_SUBDIR   "cetlvast"
)

# +--------------------------------------------------------------------------------------------------------------------+
# Because we use FetchContent_Populate to specify a source directory other than the default we have
# to manually manage the <lowercaseName>_POPULATED, <lowercaseName>_SOURCE_DIR, and <lowercaseName>_BINARY_DIR
# variables normally set by this method.
# See https://cmake.org/cmake/help/latest/module/FetchContent.html?highlight=fetchcontent#command:fetchcontent_populate
# for more information.
# This is not ideal, to copy-and-paste this code, but it is the only way to redirect fetch content to an in-source
# directory. An upstream patch to cmake is needed to fix this.
get_property(cetl_POPULATED GLOBAL PROPERTY cetl_POPULATED)

if(NOT cetl_POPULATED)

    cmake_path(APPEND CETLVAST_EXTERNAL_ROOT "cetl" OUTPUT_VARIABLE LOCAL_CETL_SOURCE_DIR)

    if (NOT ${FETCHCONTENT_FULLY_DISCONNECTED})
        FetchContent_Populate(
            cetl
            SOURCE_DIR      ${LOCAL_CETL_SOURCE_DIR}
            GIT_REPOSITORY  ${cetl_GIT_REPOSITORY}
            GIT_TAG         ${cetl_GIT_TAG}
        )
    else()
        set(cetl_SOURCE_DIR ${LOCAL_CETL_SOURCE_DIR})
    endif()

    set_property(GLOBAL PROPERTY cetl_POPULATED true)

endif()
# +--------------------------------------------------------------------------------------------------------------------+

if (NOT TARGET cetl)

# Use use CETL types in libcyphal as well as the cmake modules it defines.
cmake_path(APPEND cetl_SOURCE_DIR "cetlvast" "cmake" "modules" OUTPUT_VARIABLE LOCAL_CETL_CMAKE_MODULE_PATH)

if (EXISTS ${LOCAL_CETL_CMAKE_MODULE_PATH})
    set(cetl_FOUND TRUE)
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(cetl
    REQUIRED_VARS cetl_SOURCE_DIR cetl_FOUND
)

list(APPEND CMAKE_MODULE_PATH "${LOCAL_CETL_CMAKE_MODULE_PATH}")

include(ProjectLibrary)
add_project_library(
    NAME cetl
    HEADER_PATH
        ${cetl_SOURCE_DIR}/include/
)

endif()
