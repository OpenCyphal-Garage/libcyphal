#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

# +---------------------------------------------------------------------------+
# | STYLE
# +---------------------------------------------------------------------------+
find_package(clangformat REQUIRED)

create_check_style_target(format-check ${CETLVAST_STYLE_CHECK} "${CETL_INCLUDE}/**/*.hpp")

add_custom_target(
     suite_all
     COMMENT
        "All CETL suites define this target as a default action scripts can rely on."
     DEPENDS
        format-check
)
