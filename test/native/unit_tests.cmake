#
# Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#

#
# We generate coverage reports. Please look at them (It wasn't easy to get this to work).
#
find_package(lcov REQUIRED)

# +---------------------------------------------------------------------------+
# | BUILD NATIVE UNIT TESTS
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
     add_dependencies(${ARG_TEST_NAME} dsdl-regulated)
     target_link_libraries(${ARG_TEST_NAME} gmock_main)

     set_target_properties(${ARG_TEST_NAME}
                           PROPERTIES
                           RUNTIME_OUTPUT_DIRECTORY "${ARG_OUTDIR}"
     )

     if(ALL_NATIVE_UNIT_TESTS)
          set(ALL_NATIVE_UNIT_TESTS "${ALL_NATIVE_UNIT_TESTS} ${ARG_TEST_NAME}" PARENT_SCOPE)
     else()
          set(ALL_NATIVE_UNIT_TESTS "${ARG_TEST_NAME}" PARENT_SCOPE)
     endif()

endfunction()


#
# function: define_native_test_run - creates a makefile target that will build and
# run individual unit tests. This also properly sets up the coverage counters.
#
# param: ARG_TEST_NAME string - The name of the test to run.
# param: ARG_OUTDIR path - The path where the test binaries live.
#
function(define_native_test_run ARG_TEST_NAME ARG_OUTDIR)
     add_custom_target(
          run_${ARG_TEST_NAME}
          COMMAND # Reset coverage data
               ${LCOV}
                    ${LIBUAVCAN_GCOV_TOOL_ARG}
                    --zerocounters
                    --directory ${CMAKE_CURRENT_BINARY_DIR}
          COMMAND # Generate initial "zero coverage" data.
               ${LCOV}
                    ${LIBUAVCAN_GCOV_TOOL_ARG}
                    --capture
                    --initial
                    --directory ${CMAKE_CURRENT_BINARY_DIR}
                    --output-file ${ARG_OUTDIR}/coverage.baseline.info
          COMMAND
               ${ARG_OUTDIR}/${ARG_TEST_NAME}
          COMMAND # Generate coverage from tests.
               ${LCOV}
                    ${LIBUAVCAN_GCOV_TOOL_ARG}
                    --rc lcov_branch_coverage=1
                    --capture
                    --directory ${CMAKE_CURRENT_BINARY_DIR}
                    --test-name ${ARG_TEST_NAME}
                    --output-file ${ARG_OUTDIR}/coverage.${ARG_TEST_NAME}.test.info
          COMMAND # Combine all the test runs with the baseline
               ${LCOV}
                    ${LIBUAVCAN_GCOV_TOOL_ARG}
                    --rc lcov_branch_coverage=1
                    --add-tracefile ${ARG_OUTDIR}/coverage.baseline.info
                    --add-tracefile ${ARG_OUTDIR}/coverage.${ARG_TEST_NAME}.test.info
                    --output-file ${ARG_OUTDIR}/coverage.${ARG_TEST_NAME}.info
          COMMAND # Only use data for things under libuavcan.
               ${LCOV}
                    ${LIBUAVCAN_GCOV_TOOL_ARG}
                    --rc lcov_branch_coverage=1
                    --extract ${ARG_OUTDIR}/coverage.${ARG_TEST_NAME}.info
                    ${CMAKE_CURRENT_SOURCE_DIR}/libuavcan/include/\\*
                    --output-file ${ARG_OUTDIR}/coverage.${ARG_TEST_NAME}.filtered.info
          OUTPUT ${ARG_OUTDIR}/coverage.${ARG_TEST_NAME}.filtered.info
          DEPENDS ${ARG_TEST_NAME}
     )

endfunction()

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
          DEPENDS run_${ARG_TEST_NAME}
     )
endfunction()

# All test binaries and reports will be created under this directory.
set(LIBUAVCAN_NATIVE_TEST_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/tests)

# +---------------------------------------------------------------------------+
#   What follows are some gynastics to allow coverage reports to be generated
#   using either gcc or clang but resulting in the same .info format. The
#   consistent output is needed to ensure we can merge and compare coveage data
#   regardless of the compiler used to create the tests.

set(LIBUAVCAN_GCOV_TOOL_ARG )

if (${LIBUAVCAN_USE_LLVM_COV})
     # Try to find llvm coverage. If we don't find it
     # we'll simply omit the tool arg and hope that lcov
     # can figure it out.
     find_program(LLVM_COV NAMES llvm-cov llvm-cov-6.0)

     if (LLVM_COV)
          message(STATUS "Generating an llvm-cov wrapper to enable lcov report generation from clang output.")
          # Thanks to http://logan.tw/posts/2015/04/28/check-code-coverage-with-clang-and-lcov/
          file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/gcov_tool.sh "#!/usr/bin/env bash\nexec ${LLVM_COV} gcov \"$@\"\n")
          file(COPY ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/gcov_tool.sh
               DESTINATION ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR}
               NO_SOURCE_PERMISSIONS
               FILE_PERMISSIONS OWNER_READ
                              OWNER_WRITE
                              OWNER_EXECUTE
                              GROUP_READ
                              GROUP_EXECUTE
                              WORLD_READ
                              WORLD_EXECUTE)
          set(LIBUAVCAN_GCOV_TOOL_ARG "--gcov-tool ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR}/gcov_tool.sh")
     else()
          message(WARNING "llvm-cov was not found but we are compiling using clang. The coverage report build step may fail.")
     endif()
endif()

# +---------------------------------------------------------------------------+
#   We generate individual test binaires so we can record which test generated
#   what coverage. We also allow test authors to generate coverage reports for
#   just one test allowing for faster iteration.
file(GLOB NATIVE_TESTS
     LIST_DIRECTORIES false
     RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ${CMAKE_CURRENT_SOURCE_DIR}/test/native/test_*.cpp
)

set(ALL_TESTS "")
set(ALL_TEST_COVERAGE "")

foreach(NATIVE_TEST ${NATIVE_TESTS})
    get_filename_component(NATIVE_TEST_NAME ${NATIVE_TEST} NAME_WE)
    define_native_unit_test(${NATIVE_TEST_NAME} ${NATIVE_TEST} ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR})
    define_native_test_run(${NATIVE_TEST_NAME} ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR})
    define_natve_test_coverage(${NATIVE_TEST_NAME} ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR})
    list(APPEND ALL_TESTS "run_${NATIVE_TEST_NAME}")
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
     DEPENDS ${ALL_TESTS}
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
)

# Write a README to create the tests folder.
file(WRITE ${LIBUAVCAN_NATIVE_TEST_BINARY_DIR}/README.txt
     "All test binaries and output will appear under here.")
