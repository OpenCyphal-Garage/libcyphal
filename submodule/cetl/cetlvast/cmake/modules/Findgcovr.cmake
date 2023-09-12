#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

find_program(GCOVR gcovr)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(gcovr
    REQUIRED_VARS GCOVR
)
