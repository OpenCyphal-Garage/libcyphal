#
# Copyright Amazon.com Inc. or its affiliates.
#

create_source_external_project_adder(libcanard libcanard canard.h)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(libcanard
    REQUIRED_VARS LIBCANARD_FOUND LIBCANARD_INCLUDE_PATH LIBCANARD_EXTERNAL_PROJECT_PATH
)
