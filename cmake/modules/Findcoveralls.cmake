#
# Find our custom coveralls python upload script.
#

find_program(COVERALLS info_to_coveralls)

include(FindPackageHandleStandardArgs)
 
find_package_handle_standard_args(nnvg
    REQUIRED_VARS COVERALLS
)
