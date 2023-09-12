#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

# unit tests compiled for whatever environment the build is running on. They assume they are running
# on a fairly robust POSIX environment and use googletest/googlemock to organize the tests. Native tests should work
# on linux, osx, or Windows hosts and should work on any popular architecture including 32-bit and 64-bit ARM and x86.
# Finally, native tests assume the available toolchain can compile and run executables as part of the build process.
#

# All test binaries and reports will be created under this directory.
set(CETLVAST_NATIVE_TEST_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/cetlvast/suites/unittest)

#
# googletest (and googlemock) external project.
#
find_package(gtest REQUIRED)

# +---------------------------------------------------------------------------+
# | BUILD NATIVE UNIT TESTS
# +---------------------------------------------------------------------------+
function(get_internal_output_path_for_source ARG_SOURCEFILE ARG_ARTIFACT_SUFFIX OUTARG_INTERNAL_DIR)
     # This is a hack I don't know how to get rid of. We're not supposed to "know" about the
     # CMakeFiles directory nor its internal structure but we have to list the test binary
     # byproducts when enabling coverage to make sure the clean target works properly.
     # the use of a OBJECT library at least enforces that these intermediates are available.
     cmake_path(GET ARG_SOURCEFILE PARENT_PATH LOCAL_SOURCEFILE_REL_PATH)
     cmake_path(SET LOCAL_SOURCFILE_REL_PATH_NORMAL NORMALIZE ${LOCAL_SOURCEFILE_REL_PATH})
     cmake_path(GET ARG_SOURCEFILE STEM LOCAL_SOURCEFILE_NAME)
     cmake_path(SET LOCAL_RESULT "CMakeFiles")
     cmake_path(APPEND LOCAL_RESULT "${LOCAL_SOURCEFILE_NAME}${ARG_ARTIFACT_SUFFIX}.dir")
     cmake_path(APPEND LOCAL_RESULT "${LOCAL_SOURCFILE_REL_PATH_NORMAL}")
     set(${OUTARG_INTERNAL_DIR} ${LOCAL_RESULT} PARENT_SCOPE)
endfunction()

#
# function: define_native_unit_test - Creates rules and targets to build and run gtest-based
#           unit tests.
#
# param: ARG_TEST_SOURCE path      - A single source file that is the test main.
# param: ARG_OUTDIR path           - A path to output test binaries and coverage data under.
# param: OUTARG_TESTNAME path      - Set to the name of the test derived from the given source
#                                    file.
# param: OUTARG_TESTRESULT path    - Set to the path for the test result file produced by a
#                                    successful test run.
#
function(define_native_gtest_unit_test ARG_TEST_SOURCE ARG_OUTDIR OUTARG_TESTNAME OUTARG_TESTRESULT)

     cmake_path(GET ARG_TEST_SOURCE STEM LOCAL_TEST_NAME)
     set(LOCAL_TESTRESULT ${ARG_OUTDIR}/${LOCAL_TEST_NAME}-gtest.xml)

     message(STATUS "Defining googletest binary ${LOCAL_TEST_NAME} for source file ${ARG_TEST_SOURCE}")

     # Create explicit object file target so we can find it.
     add_library(${LOCAL_TEST_NAME}_objlib OBJECT ${ARG_TEST_SOURCE})
     # This gets the includes from the gmock_main interface library.
     target_link_libraries(${LOCAL_TEST_NAME}_objlib gmock_main)

     add_executable(${LOCAL_TEST_NAME} $<TARGET_OBJECTS:${LOCAL_TEST_NAME}_objlib>)
     target_link_libraries(${LOCAL_TEST_NAME} gmock_main)
     set_target_properties(${LOCAL_TEST_NAME}
                           PROPERTIES
                           RUNTIME_OUTPUT_DIRECTORY "${ARG_OUTDIR}"
     )

     add_custom_command(
          OUTPUT ${LOCAL_TESTRESULT}
          COMMAND ${ARG_OUTDIR}/${LOCAL_TEST_NAME} --gtest_output=xml:${LOCAL_TESTRESULT}
          DEPENDS ${ARG_OUTDIR}/${LOCAL_TEST_NAME}
          WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
     )

     message(DEBUG "Using --getst_output=xml: expecting test to generate: ${LOCAL_TESTRESULT}")

     set(LOCAL_BYPRODUCTS "")

     if (CETLVAST_ENABLE_COVERAGE)

          get_internal_output_path_for_source(${ARG_TEST_SOURCE} "_objlib" LOCAL_OBJLIB_FOLDER_REL)
          cmake_path(ABSOLUTE_PATH LOCAL_OBJLIB_FOLDER_REL
                     BASE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                     OUTPUT_VARIABLE LOCAL_OBJLIB_FOLDER)

          cmake_path(GET ARG_TEST_SOURCE EXTENSION LOCAL_TEST_EXT)
          # the generation of gcda files assumes "-fprofile-argcs" (or "-coverage" which includes this flag).
          set(LOCAL_GCDA_FILEPATH "${LOCAL_OBJLIB_FOLDER}/${LOCAL_TEST_NAME}${LOCAL_TEST_EXT}.gcda")
          # the generation of gcno files assumes "-ftest-coverage" (or "-coverage" which includes this flag)
          set(LOCAL_GCNO_FILEPATH "${LOCAL_OBJLIB_FOLDER}/${LOCAL_TEST_NAME}${LOCAL_TEST_EXT}.gcno")

          list(APPEND LOCAL_BYPRODUCTS ${LOCAL_GCDA_FILEPATH})
          list(APPEND LOCAL_BYPRODUCTS ${LOCAL_GCNO_FILEPATH})

          message(DEBUG "Coverage is enabled: expecting test run byproduct: ${LOCAL_GCDA_FILEPATH}")
          message(DEBUG "Coverage is enabled: expecting test run byproduct: ${LOCAL_GCNO_FILEPATH}")
     endif()

     add_custom_target(
          run_${LOCAL_TEST_NAME}
          COMMAND ${ARG_OUTDIR}/${LOCAL_TEST_NAME}
          DEPENDS ${ARG_OUTDIR}/${LOCAL_TEST_NAME}
          BYPRODUCTS ${LOCAL_BYPRODUCTS}
     )

     set(${OUTARG_TESTNAME} ${LOCAL_TEST_NAME} PARENT_SCOPE)
     set(${OUTARG_TESTRESULT} ${LOCAL_TESTRESULT} PARENT_SCOPE)

endfunction()

# +---------------------------------------------------------------------------+
#   We generate individual test binaires so we can record which test generated
#   what coverage. We also allow test authors to generate coverage reports for
#   just one test allowing for faster iteration.
file(GLOB NATIVE_TESTS
     LIST_DIRECTORIES false
     RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ${CMAKE_CURRENT_SOURCE_DIR}/suites/unittest/test_*.cpp
)

set(ALL_TESTS_BUILD "")
set(ALL_TESTS "")
set(LOCAL_TEST_NAME "")
set(LOCAL_TEST_REPORT "")

foreach(NATIVE_TEST ${NATIVE_TESTS})
     define_native_gtest_unit_test(${NATIVE_TEST}
                                   ${CETLVAST_NATIVE_TEST_BINARY_DIR}
                                   LOCAL_TEST_NAME
                                   LOCAL_TEST_REPORT)
    list(APPEND ALL_TESTS_BUILD "${CETLVAST_NATIVE_TEST_BINARY_DIR}/${LOCAL_TEST_NAME}")
    list(APPEND ALL_TESTS "${LOCAL_TEST_REPORT}")
endforeach()

add_custom_target(
     build_all
     DEPENDS ${ALL_TESTS_BUILD}
)

if (CETLVAST_ENABLE_COVERAGE)

message(STATUS "Coverage is enabled: adding coverage targets.")

# +---------------------------------------------------------------------------+
#   If coverage is enabled we have more work to do...
# +---------------------------------------------------------------------------+

# we use gcovr to support standard coverage reporting tools like coveralls or sonarqube.
find_package(gcovr REQUIRED)

function(define_gcovr_tracefile_for_test ARG_TEST_SOURCE ARG_OUT_TRACEFILE)
     # We're not supposed to know what the test name is given the source file. This
     # needs to be cleaned up if we want to reuse it.
     cmake_path(GET ARG_TEST_SOURCE STEM LOCAL_TEST_NAME)
     cmake_path(GET ARG_TEST_SOURCE EXTENSION LOCAL_TEST_EXT)

     get_internal_output_path_for_source(${ARG_TEST_SOURCE} "_objlib" LOCAL_OBJLIB_REL_FOLDER)

     set(LOCAL_TRACEFILE_NAME "${LOCAL_TEST_NAME}-gcovr.json")
     set(LOCAL_TRACEFILE_PATH "${CETLVAST_NATIVE_TEST_BINARY_DIR}/${LOCAL_TRACEFILE_NAME}")
     set(LOCAL_TESTRESULT "${CETLVAST_NATIVE_TEST_BINARY_DIR}/${LOCAL_TEST_NAME}-gtest.xml")

     add_custom_command(
          COMMAND # Generate tracefile from tests.
               ${GCOVR}
                    --gcov-executable ${CMAKE_C_COVERAGE_PROCESSOR}
                    --r ${CETL_ROOT}
                    --json ${LOCAL_TRACEFILE_PATH}
                    --exclude "\"${EXTERNAL_PROJECT_DIRECTORY}\""
                    --gcov-exclude "\"${EXTERNAL_PROJECT_DIRECTORY}\""
                    --object-directory ${LOCAL_OBJLIB_REL_FOLDER}
                    ${LOCAL_OBJLIB_REL_FOLDER}
          WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
          OUTPUT ${LOCAL_TRACEFILE_PATH}
          DEPENDS ${LOCAL_TESTRESULT}
     )

     message(DEBUG "Will generate tracefile \"${LOCAL_TRACEFILE_PATH}\" for instrumentation found under \"${CMAKE_CURRENT_BINARY_DIR}/${LOCAL_OBJLIB_REL_FOLDER}\"")
     set(${ARG_OUT_TRACEFILE} ${LOCAL_TRACEFILE_PATH} PARENT_SCOPE)

     add_custom_target(
          create_${LOCAL_TEST_NAME}_tracefile
          DEPENDS ${LOCAL_TRACEFILE_PATH}
     )
endfunction()

set(ALL_TEST_TRACEFILES)
set(ALL_TEST_COVERAGE "")
set(LOCAL_TRACEFILE "")
# we reset the tests to run by way of the coverage report.
set(ALL_TESTS "")

foreach(NATIVE_TEST ${NATIVE_TESTS})
    define_gcovr_tracefile_for_test(${NATIVE_TEST} NATIVE_TEST_TRACEFILE)
    list(APPEND ALL_TEST_TRACEFILES "${NATIVE_TEST_TRACEFILE}")
    list(APPEND ALL_TEST_COVERAGE "--add-tracefile")
    list(APPEND ALL_TEST_COVERAGE "${NATIVE_TEST_TRACEFILE}")
endforeach()

add_custom_command(
     OUTPUT ${CETLVAST_NATIVE_TEST_BINARY_DIR}/gcovr_html/coverage.html
     COMMAND
          ${GCOVR}
               ${ALL_TEST_COVERAGE}
               --r ${CETL_ROOT}
               --html-details ${CETLVAST_NATIVE_TEST_BINARY_DIR}/gcovr_html/coverage.html
     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
     DEPENDS ${ALL_TEST_TRACEFILES}
)

add_custom_target(
     gcovr_html_report
     DEPENDS ${CETLVAST_NATIVE_TEST_BINARY_DIR}/gcovr_html/coverage.html
)

add_custom_command(
     OUTPUT ${CETLVAST_NATIVE_TEST_BINARY_DIR}/coverage.xml
     COMMAND
          ${GCOVR}
               ${ALL_TEST_COVERAGE}
               --r ${CETL_ROOT}
               --sonarqube ${CETLVAST_NATIVE_TEST_BINARY_DIR}/coverage.xml
     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
     DEPENDS ${ALL_TEST_TRACEFILES}
)

add_custom_target(
     gcovr_sonarqube_report
     DEPENDS ${CETLVAST_NATIVE_TEST_BINARY_DIR}/coverage.xml
)

if (${CETLVAST_COVERAGE_REPORT_FORMAT} STREQUAL "html")
     # Write a README to create the gcovr_html folder.
     file(WRITE ${CETLVAST_NATIVE_TEST_BINARY_DIR}/gcovr_html/README.txt
          "gcovr html coverage report.")
     list(APPEND ALL_TESTS "${CETLVAST_NATIVE_TEST_BINARY_DIR}/gcovr_html/coverage.html")
else()
     list(APPEND ALL_TESTS "${CETLVAST_NATIVE_TEST_BINARY_DIR}/coverage.xml")
endif()

endif() # endif coverage is enabled

# +---------------------------------------------------------------------------+

add_custom_target(
     test_all
     DEPENDS
          ${ALL_TESTS}
)

add_custom_target(
     suite_all
     COMMENT
        "All CETL suites define this target as a default action scripts can rely on."
     DEPENDS
          ${ALL_TESTS}
)

# Write a README to create the tests folder.
file(WRITE ${CETLVAST_NATIVE_TEST_BINARY_DIR}/README.txt
     "All test binaries and output will appear under here.")
