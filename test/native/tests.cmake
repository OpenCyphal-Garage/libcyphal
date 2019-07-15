#
# Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#

# All test binaries and reports will be created under this directory.
set(LIBUAVCAN_NATIVE_TEST_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/tests)

#
# We generate coverage reports. Please look at them (It wasn't easy to get this to work).
#
find_package(lcov REQUIRED)

# +---------------------------------------------------------------------------+
# | BUILD NATIVE UNIT TESTS AND EXAMPLES
# +---------------------------------------------------------------------------+

#
# function: define_native_unit_test - creates an executable target and links it
# to the "all" target to build a gtest binary for the given test source.
#
# param: ARG_TEST_NAME string - The name to give the test binary.
# param: ARG_TEST_SOURCE List[path] - A list of source files to compile into
#                               the test binary.
# param: ARG_OUTDIR path - A path to output test binaries and coverage data under.
#
function(define_native_unit_test ARG_TEST_NAME ARG_TEST_SOURCE ARG_OUTDIR)

     add_executable(${ARG_TEST_NAME} ${ARG_TEST_SOURCE})
     target_link_libraries(${ARG_TEST_NAME} dsdl-regulated)
     target_link_libraries(${ARG_TEST_NAME} gmock_main)

     set_target_properties(${ARG_TEST_NAME}
                           PROPERTIES
                           RUNTIME_OUTPUT_DIRECTORY "${ARG_OUTDIR}"
     )

endfunction()


#
# function: define_native_test_run - creates a makefile target that will build and
# run individual unit tests.
#
# param: ARG_TEST_NAME string - The name of the test to run. A target will be created
#                          with the name run_${ARG_TEST_NAME}
# param: ARG_OUTDIR path - The path where the test binaries live.
#
function(define_native_test_run ARG_TEST_NAME ARG_OUTDIR)
     add_custom_target(
          run_${ARG_TEST_NAME}
          COMMAND
               ${ARG_OUTDIR}/${ARG_TEST_NAME}
          DEPENDS
               ${ARG_TEST_NAME}
     )

endfunction()

# +---------------------------------------------------------------------------+
#   We generate individual test binaires so we can record which test generated
#   what coverage. We also allow test authors to generate coverage reports for
#   just one test allowing for faster iteration.
file(GLOB NATIVE_TESTS
     LIST_DIRECTORIES false
     RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ${CMAKE_CURRENT_SOURCE_DIR}/test/native/test_*.cpp
)

add_custom_target(
     lcov_zero
     ${LCOV}
          ${LIBUAVCAN_GCOV_TOOL_ARG}
          --zerocounters
          --directory ${CMAKE_CURRENT_BINARY_DIR}
     COMMENT "Resetting coverage counters."
)

set(ALL_TESTS "")
set(ALL_TESTS_WITH_LCOV "")
set(ALL_TEST_COVERAGE "")

foreach(NATIVE_TEST ${NATIVE_TESTS})
    get_filename_component(NATIVE_TEST_NAME ${NATIVE_TEST} NAME_WE)
    define_native_unit_test(${NATIVE_TEST_NAME} ${NATIVE_TEST} ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR})
    define_native_test_run(${NATIVE_TEST_NAME} ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR})
    define_native_test_run_with_lcov(${NATIVE_TEST_NAME} ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR})
    define_natve_test_coverage(${NATIVE_TEST_NAME} ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR})
    list(APPEND ALL_TESTS "run_${NATIVE_TEST_NAME}")
    list(APPEND ALL_TESTS_WITH_LCOV "run_${NATIVE_TEST_NAME}_with_lcov")
    list(APPEND ALL_TEST_COVERAGE "--add-tracefile")
    list(APPEND ALL_TEST_COVERAGE "${LIBUAVCAN_NATIVE_TEST_BINARY_DIR}/coverage.${NATIVE_TEST_NAME}.filtered.info")
endforeach()

# +---------------------------------------------------------------------------+
#   Finally, we setup an overall report. the coverage.info should be uploaded
#   to a coverage reporting service as part of the CI pipeline.

add_custom_command(
     OUTPUT ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR}/coverage.all.info
     COMMAND
          ${LCOV}
               ${LIBUAVCAN_GCOV_TOOL_ARG}
               --rc lcov_branch_coverage=1
               ${ALL_TEST_COVERAGE}
               --output-file ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR}/coverage.all.info
     DEPENDS ${ALL_TESTS_WITH_LCOV}
)

add_custom_command(
     OUTPUT ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR}/coverage.info
     COMMAND
          ${LCOV}
               ${LIBUAVCAN_GCOV_TOOL_ARG}
               --rc lcov_branch_coverage=1
               --extract ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR}/coverage.all.info
                         ${CMAKE_CURRENT_SOURCE_DIR}/libuavcan/include/\\*
               --output-file ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR}/coverage.info
     DEPENDS ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR}/coverage.all.info
)

add_custom_target(
     cov_info
     DEPENDS ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR}/coverage.info
)

add_custom_target(
     cov_all
     ${GENHTML} --title "${PROJECT_NAME} native test coverage"
          --output-directory ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR}/coverage/all
          --demangle-cpp
          --sort
          --num-spaces 4
          --function-coverage
          --branch-coverage
          --legend
          --highlight
          --show-details
          ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR}/coverage.info
     DEPENDS ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR}/coverage.info
     COMMENT "Build and run all tests and generate an overall html coverage report."
)

add_custom_target(
     test_all
     DEPENDS
          ${ALL_TESTS}
)

# +---------------------------------------------------------------------------+
#   If we can we try to upload the report to coveralls.
#
find_package(coveralls)

if (COVERALLS)
     message(STATUS "coveralls upload binary was defined. Adding the upload target: coveralls_upload")
     add_custom_target(
          coveralls_upload
          ${COVERALLS} --root ${CMAKE_CURRENT_SOURCE_DIR} ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR}/coverage.info -v
          COMMENT "Upload to coveralls (requires that COVERALLS_TOKEN is defined in the environment)."
     )

else()
     message(WARNING "coveralls upload script not found. Upload target will not be available.")
endif()


# Write a README to create the tests folder.
file(WRITE ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR}/README.txt
     "All test binaries and output will appear under here.")
