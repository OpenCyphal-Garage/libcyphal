#
# Copyright Amazon.com Inc. or its affiliates.
#

create_source_external_project_adder(cetl include/cetl cetl.hpp)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(cetl
    REQUIRED_VARS CETL_FOUND CETL_INCLUDE_PATH CETL_EXTERNAL_PROJECT_PATH
)
