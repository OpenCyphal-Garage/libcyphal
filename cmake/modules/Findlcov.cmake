
find_program(LCOV lcov)
find_program(GENHTML genhtml)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(lcov
    LCOV_FOUND
)

find_package_handle_standard_args(genhtml
    GENHTML_FOUND
)
