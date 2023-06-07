#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

if(NOT TARGET udpard)
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

# The automatic management of the <lowercase>_POPULATED name appears to be broken in
# cmake 3.21 and earlier.  This workaround may not be needed after 3.24.
get_property(libudpard_POPULATED GLOBAL PROPERTY libudpard_POPULATED)

if(NOT libudpard_POPULATED)

    if (NOT FETCHCONTENT_SOURCE_DIR_libudpard)
        set(FETCHCONTENT_SOURCE_DIR_libudpard ${CMAKE_SOURCE_DIR}/external/libudpard)
    endif()

    if (NOT ${FETCHCONTENT_FULLY_DISCONNECTED})
        FetchContent_Populate(
            libudpard
            SOURCE_DIR      ${FETCHCONTENT_SOURCE_DIR_libudpard}
            GIT_REPOSITORY  ${libudpard_GIT_REPOSITORY}
            GIT_TAG         ${libudpard_GIT_TAG}
        )
    else()
        set(libudpard_SOURCE_DIR ${FETCHCONTENT_SOURCE_DIR_libudpard})
    endif()

    # The automatic management of the <lowercase>_POPULATED name appears to be broken in
    # cmake 3.21 and earlier.  This workaround may not be needed after 3.24.
    set_property(GLOBAL PROPERTY libudpard_POPULATED true)

    find_package_handle_standard_args(libudpard
        REQUIRED_VARS libudpard_SOURCE_DIR
    )

    add_project_library(
        NAME udpard
        SOURCES
            ${libudpard_SOURCE_DIR}/libudpard/udpard.c
        HEADER_PATH
            ${libudpard_SOURCE_DIR}/libudpard/
        HEADER_INCLUDE_PATTERNS
            *.h
        STATIC
        FPIC
    )

endif()
endif()
