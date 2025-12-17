#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

include(FetchContent)
set(cavl_GIT_REPOSITORY "https://github.com/pavel-kirienko/cavl")
set(cavl_GIT_TAG "c-2.1.0")

FetchContent_Declare(
    cavl
    GIT_REPOSITORY ${cavl_GIT_REPOSITORY}
    GIT_TAG ${cavl_GIT_TAG}
)

# +--------------------------------------------------------------------------------------------------------------------+
# Because we use FetchContent_Populate to specify a source directory other than the default we have
# to manually manage the <lowercaseName>_POPULATED, <lowercaseName>_SOURCE_DIR, and <lowercaseName>_BINARY_DIR
# variables normally set by this method.
# See https://cmake.org/cmake/help/latest/module/FetchContent.html?highlight=fetchcontent#command:fetchcontent_populate
# for more information.
# This is not ideal, to copy-and-paste this code, but it is the only way to redirect fetch content to an in-source
# directory. An upstream patch to cmake is needed to fix this.
get_property(cavl_POPULATED GLOBAL PROPERTY cavl_POPULATED)
if(NOT cavl_POPULATED)
    cmake_path(APPEND CETLVAST_EXTERNAL_ROOT "cavl" OUTPUT_VARIABLE LOCAL_cavl_SOURCE_DIR)
    if (NOT ${FETCHCONTENT_FULLY_DISCONNECTED})
        FetchContent_Populate(
            cavl
            SOURCE_DIR      ${LOCAL_cavl_SOURCE_DIR}
            GIT_REPOSITORY  ${cavl_GIT_REPOSITORY}
            GIT_TAG         ${cavl_GIT_TAG}
        )
    else()
        set(cavl_SOURCE_DIR ${LOCAL_cavl_SOURCE_DIR})
    endif()
    set_property(GLOBAL PROPERTY cavl_POPULATED true)
endif()

add_project_library(
    NAME cavl2_c
    HEADER_PATH "${cavl_SOURCE_DIR}/c"
)
