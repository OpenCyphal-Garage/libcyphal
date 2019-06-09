#
# Find our custom coveralls python upload script.
#

find_program(COVERALLS info_to_coveralls)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(info_to_coveralls
    REQUIRED_VARS COVERALLS
)
