#
# Copyright Amazon.com Inc. or its affiliates.
#

create_source_external_project_adder(o1heap o1heap o1heap.h)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(o1heap
    REQUIRED_VARS O1HEAP_FOUND O1HEAP_INCLUDE_PATH O1HEAP_EXTERNAL_PROJECT_PATH
)
