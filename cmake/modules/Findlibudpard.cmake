#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

include(FetchContent)
include(FindPackageHandleStandardArgs)
include(ProjectLibrary)

set(libudpard_GIT_REPOSITORY "https://github.com/OpenCyphal-garage/libudpard.git")
set(libudpard_GIT_TAG "89f87abf000cbe358b8e4974a52793c6dea12bd4")

FetchContent_Declare(
    libudpard
    GIT_REPOSITORY  ${libudpard_GIT_REPOSITORY}
    GIT_TAG         ${libudpard_GIT_TAG}
)
# +--------------------------------------------------------------------------------------------------------------------+
# Because we use FetchContent_Populate to specify a source directory other than the default we have
# to manually manage the <lowercaseName>_POPULATED, <lowercaseName>_SOURCE_DIR, and <lowercaseName>_BINARY_DIR
# variables normally set by this method.
# See https://cmake.org/cmake/help/latest/module/FetchContent.html?highlight=fetchcontent#command:fetchcontent_populate
# for more information.
# This is not ideal, to copy-and-paste this code, but it is the only way to redirect fetch content to an in-source
# directory. An upstream patch to cmake is needed to fix this.
get_property(libudpard_POPULATED GLOBAL PROPERTY libudpard_POPULATED)

if(NOT libudpard_POPULATED)

    cmake_path(APPEND CETLVAST_EXTERNAL_ROOT "libudpard" OUTPUT_VARIABLE LOCAL_libudpard_SOURCE_DIR)

    if (NOT ${FETCHCONTENT_FULLY_DISCONNECTED})
        FetchContent_Populate(
            libudpard
            SOURCE_DIR      ${LOCAL_libudpard_SOURCE_DIR}
            GIT_REPOSITORY  ${libudpard_GIT_REPOSITORY}
            GIT_TAG         ${libudpard_GIT_TAG}
        )
    else()
        set(libudpard_SOURCE_DIR ${LOCAL_libudpard_SOURCE_DIR})
    endif()

    set_property(GLOBAL PROPERTY libudpard_POPULATED true)

endif()
# +--------------------------------------------------------------------------------------------------------------------+

if(NOT TARGET udpard)

cmake_path(APPEND libudpard_SOURCE_DIR "libudpard" OUTPUT_VARIABLE LOCAL_LIBUDPARD_SOURCE_DIR)

if (EXISTS ${LOCAL_LIBUDPARD_SOURCE_DIR})
    set(libudpard_FOUND TRUE)
endif()

include(FindPackageHandleStandardArgs)
include(ProjectLibrary)

find_package_handle_standard_args(libudpard
    REQUIRED_VARS libudpard_SOURCE_DIR libudpard_FOUND
)

add_project_library(
    NAME udpard
    SOURCES
        ${LOCAL_LIBUDPARD_SOURCE_DIR}/udpard.c
    HEADER_PATH
        ${LOCAL_LIBUDPARD_SOURCE_DIR}
    HEADER_INCLUDE_PATTERNS
        *.h
    STATIC
    FPIC
)

endif()
