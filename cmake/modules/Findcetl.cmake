#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

include(FindPackageHandleStandardArgs)
include(ProjectLibrary)

if (EXISTS ${CETLVAST_EXTERNAL_ROOT}/cetl)
    set(cetl_SOURCE_DIR ${CETLVAST_EXTERNAL_ROOT}/cetl)
endif()

find_package_handle_standard_args(cetl
    REQUIRED_VARS cetl_SOURCE_DIR
)

if (NOT TARGET cetl)
add_project_library(
    NAME cetl
    HEADER_PATH
        ${cetl_SOURCE_DIR}/include/
)
endif()
