#
# Copyright Amazon.com Inc. or its affiliates.
#

create_source_external_project_adder(libudpard libudpard udpard.h)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(libudpard
    REQUIRED_VARS LIBUDPARD_FOUND LIBUDPARD_INCLUDE_PATH LIBUDPARD_EXTERNAL_PROJECT_PATH
)
