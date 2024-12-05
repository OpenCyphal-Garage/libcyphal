#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#



include(FetchContent)

set(libcanard_GIT_REPOSITORY "https://github.com/OpenCyphal/libcanard.git")
set(libcanard_GIT_TAG "sshirokov/v4_tx_poll")

FetchContent_Declare(
    libcanard
    GIT_REPOSITORY  ${libcanard_GIT_REPOSITORY}
    GIT_TAG         ${libcanard_GIT_TAG}
)
# +--------------------------------------------------------------------------------------------------------------------+
# Because we use FetchContent_Populate to specify a source directory other than the default we have
# to manually manage the <lowercaseName>_POPULATED, <lowercaseName>_SOURCE_DIR, and <lowercaseName>_BINARY_DIR
# variables normally set by this method.
# See https://cmake.org/cmake/help/latest/module/FetchContent.html?highlight=fetchcontent#command:fetchcontent_populate
# for more information.
# This is not ideal, to copy-and-paste this code, but it is the only way to redirect fetch content to an in-source
# directory. An upstream patch to cmake is needed to fix this.
get_property(libcanard_POPULATED GLOBAL PROPERTY libcanard_POPULATED)

if(NOT libcanard_POPULATED)

    cmake_path(APPEND CETLVAST_EXTERNAL_ROOT "libcanard" OUTPUT_VARIABLE LOCAL_libcanard_SOURCE_DIR)

    if (NOT ${FETCHCONTENT_FULLY_DISCONNECTED})
        FetchContent_Populate(
            libcanard
            SOURCE_DIR      ${LOCAL_libcanard_SOURCE_DIR}
            GIT_REPOSITORY  ${libcanard_GIT_REPOSITORY}
            GIT_TAG         ${libcanard_GIT_TAG}
        )
    else()
        set(libcanard_SOURCE_DIR ${LOCAL_libcanard_SOURCE_DIR})
    endif()

    set_property(GLOBAL PROPERTY libcanard_POPULATED true)

endif()
# +--------------------------------------------------------------------------------------------------------------------+


if(NOT TARGET canard)

cmake_path(APPEND libcanard_SOURCE_DIR "libcanard" OUTPUT_VARIABLE LOCAL_LIBCANARD_SOURCE_DIR)

if (EXISTS ${LOCAL_LIBCANARD_SOURCE_DIR})
    set(libcanard_FOUND TRUE)
endif()

include(FindPackageHandleStandardArgs)
include(ProjectLibrary)

find_package_handle_standard_args(libcanard
    REQUIRED_VARS libcanard_SOURCE_DIR libcanard_FOUND
)

add_project_library(
    NAME canard
    SOURCES
        ${LOCAL_LIBCANARD_SOURCE_DIR}/canard.c
    HEADER_PATH
        ${LOCAL_LIBCANARD_SOURCE_DIR}
    HEADER_INCLUDE_PATTERNS
        *.h
    STATIC
    FPIC
)

endif()