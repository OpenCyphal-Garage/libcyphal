#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

if(NOT TARGET canard)

include(FetchContent)
include(FindPackageHandleStandardArgs)
include(ProjectLibrary)

set(libcanard_GIT_REPOSITORY "https://github.com/OpenCyphal/libcanard.git")
set(libcanard_GIT_TAG "69ed329db4ae31d9c85dbe052434f60e552cecbe")

FetchContent_Declare(
    libcanard
    GIT_REPOSITORY  ${libcanard_GIT_REPOSITORY}
    GIT_TAG         ${libcanard_GIT_TAG}
)

# The automatic management of the <lowercase>_POPULATED name appears to be broken in
# cmake 3.21 and earlier.  This workaround may not be needed after 3.24.
get_property(libcanard_POPULATED GLOBAL PROPERTY libcanard_POPULATED)

if(NOT libcanard_POPULATED)

    if (NOT FETCHCONTENT_SOURCE_DIR_libcanard)
        set(FETCHCONTENT_SOURCE_DIR_libcanard ${CMAKE_SOURCE_DIR}/external/libcanard)
    endif()

    if (NOT ${FETCHCONTENT_FULLY_DISCONNECTED})
        FetchContent_Populate(
            libcanard
            SOURCE_DIR      ${FETCHCONTENT_SOURCE_DIR_libcanard}
            GIT_REPOSITORY  ${libcanard_GIT_REPOSITORY}
            GIT_TAG         ${libcanard_GIT_TAG}
        )
    else()
        set(libcanard_SOURCE_DIR ${FETCHCONTENT_SOURCE_DIR_libcanard})
    endif()

    # The automatic management of the <lowercase>_POPULATED name appears to be broken in
    # cmake 3.21 and earlier.  This workaround may not be needed after 3.24.
    set_property(GLOBAL PROPERTY libcanard_POPULATED true)

    find_package_handle_standard_args(libcanard
        REQUIRED_VARS libcanard_SOURCE_DIR
    )

    add_project_library(
        NAME canard
        SOURCES
            ${libcanard_SOURCE_DIR}/libcanard/canard.c
        HEADER_PATH
            ${libcanard_SOURCE_DIR}/libcanard/
        HEADER_INCLUDE_PATTERNS
            *.h
        STATIC
        FPIC
    )

endif()
endif()