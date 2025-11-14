#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

include(FetchContent)

set(cavl_GIT_REPOSITORY "https://github.com/pavel-kirienko/cavl")
set(cavl_GIT_TAG "c-2.0.0")

FetchContent_Declare(
    cavl
    GIT_REPOSITORY  ${cavl_GIT_REPOSITORY}
    GIT_TAG         ${cavl_GIT_TAG}
)
FetchContent_GetProperties(cavl)
if (NOT cavl_POPULATED)
    FetchContent_Populate(cavl)
endif ()


#add_library(cavl2_c INTERFACE)
#target_include_directories(cavl2_c INTERFACE "${cavl_SOURCE_DIR}/c")
add_project_library(
    NAME cavl2_c
    HEADER_PATH "${cavl_SOURCE_DIR}/c"
)
