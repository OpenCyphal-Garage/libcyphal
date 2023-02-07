
find_program(GENHTML genhtml)

if(GENHTML)
    #
    # function: define_natve_test_coverage - creates makefile targets to generate coverage
    # data for an individual test.
    #
    # param: ARG_TEST_NAME string - The name of the test to generate coverage data for.
    # param: ARG_OUTDIR path - The path where the test binaries live and where the coverage
    #                          data will be stored.
    #
    function(define_natve_test_coverage ARG_TEST_NAME ARG_OUTDIR)

        add_custom_target(
            cov_${ARG_TEST_NAME}
            COMMAND
                ${GENHTML} --title "${ARG_TEST_NAME} coverage"
                        --output-directory ${ARG_OUTDIR}/coverage/${ARG_TEST_NAME}
                        --demangle-cpp
                        --sort
                        --num-spaces 4
                        --function-coverage
                        --branch-coverage
                        --legend
                        --highlight
                        --show-details
                        ${ARG_OUTDIR}/coverage.${ARG_TEST_NAME}.filtered.info
            DEPENDS run_${ARG_TEST_NAME}_with_lcov
        )
    endfunction()
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(genhtml
    REQUIRED_VARS GENHTML
)
