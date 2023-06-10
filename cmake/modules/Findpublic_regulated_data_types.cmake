#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

include(FetchContent)

set(public_regulated_data_types_GIT_REPOSITORY "https://github.com/OpenCyphal/public_regulated_data_types.git")
set(public_regulated_data_types_GIT_TAG "935973babe11755d8070e67452b3508b4b6833e2")

FetchContent_Declare(
    public_regulated_data_types
    GIT_REPOSITORY  ${public_regulated_data_types_GIT_REPOSITORY}
    GIT_TAG         ${public_regulated_data_types_GIT_TAG}
)

# +--------------------------------------------------------------------------------------------------------------------+
# Because we use FetchContent_Populate to specify a source directory other than the default we have
# to manually manage the <lowercaseName>_POPULATED, <lowercaseName>_SOURCE_DIR, and <lowercaseName>_BINARY_DIR
# variables normally set by this method.
# See https://cmake.org/cmake/help/latest/module/FetchContent.html?highlight=fetchcontent#command:fetchcontent_populate
# for more information.
# This is not ideal, to copy-and-paste this code, but it is the only way to redirect fetch content to an in-source
# directory. An upstream patch to cmake is needed to fix this.
get_property(public_regulated_data_types_POPULATED GLOBAL PROPERTY public_regulated_data_types_POPULATED)

if(NOT public_regulated_data_types_POPULATED)

    cmake_path(APPEND CETLVAST_EXTERNAL_ROOT "public_regulated_data_types" OUTPUT_VARIABLE LOCAL_public_regulated_data_types_SOURCE_DIR)

    if (NOT ${FETCHCONTENT_FULLY_DISCONNECTED})
        FetchContent_Populate(
            public_regulated_data_types
            SOURCE_DIR      ${LOCAL_public_regulated_data_types_SOURCE_DIR}
            GIT_REPOSITORY  ${public_regulated_data_types_GIT_REPOSITORY}
            GIT_TAG         ${public_regulated_data_types_GIT_TAG}
        )
    else()
        set(public_regulated_data_types_SOURCE_DIR ${LOCAL_public_regulated_data_types_SOURCE_DIR})
    endif()

    set_property(GLOBAL PROPERTY public_regulated_data_types_POPULATED true)

endif()
# +--------------------------------------------------------------------------------------------------------------------+

cmake_path(APPEND public_regulated_data_types_SOURCE_DIR "uavcan" OUTPUT_VARIABLE LOCAL_PRDT_SOURCE_DIR)

if (EXISTS ${LOCAL_PRDT_SOURCE_DIR})
    set(public_regulated_data_types_FOUND TRUE)
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(public_regulated_data_types
    REQUIRED_VARS public_regulated_data_types_SOURCE_DIR public_regulated_data_types_FOUND
)
