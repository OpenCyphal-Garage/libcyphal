#
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.22.0)

project(libcyphal_test_unittest CXX)

find_package(cyphal REQUIRED)
find_package(googletest REQUIRED)
if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
find_package(gcovr REQUIRED)
endif()


# +---------------------------------------------------------------------------+
#   We generate individual test binaires so we can record which test generated
#   what coverage. We also allow test authors to generate coverage reports for
#   just one test allowing for faster iteration.
file(GLOB_RECURSE NATIVE_TESTS
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    test_*.cpp **/test_*.cpp
)

set(ALL_TESTS_BUILD "")
set(ALL_TESTS_RUN "")

foreach(NATIVE_TEST ${NATIVE_TESTS})
    define_native_gtest_unittest_targets(
        TEST_SOURCE ${NATIVE_TEST}
        EXTRA_TEST_LIBS
            cetl
            cyphal
        LINK_TO_MAIN
        OUT_TEST_LIB_VARIABLE LOCAL_TEST_LIB
        OUT_TEST_EXE_VARIABLE LOCAL_TEST_TARGET
        OUT_TEST_REPORT_VARIABLE LOCAL_TEST_REPORT
    )
    set_target_properties(
        ${LOCAL_TEST_LIB}
        PROPERTIES
        CXX_CLANG_TIDY "${clang_tidy}"
    )

    list(APPEND ALL_TESTS_BUILD ${LOCAL_TEST_TARGET})
    list(APPEND ALL_TESTS_RUN ${LOCAL_TEST_REPORT})

    # We need to exclude the "external" directory from coverage reports.
    cmake_path(APPEND LIBCYPHAL_ROOT "external" OUTPUT_VARIABLE LIBCYPHAL_EXTERNAL_PATH)
    
    if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
        define_gcovr_tracefile_target(
            TARGET ${LOCAL_TEST_TARGET}
            ROOT_DIRECTORY ${LIBCYPHAL_ROOT}
            TARGET_EXECUTION_DEPENDS ${LOCAL_TEST_REPORT}
            OBJECT_LIBRARY ${LOCAL_TEST_LIB}
            EXCLUDE_PATHS ${LIBCYPHAL_EXTERNAL_PATH}
            EXCLUDE_TEST_FRAMEWORKS
            EXCLUDE_TARGET
            ENABLE_INSTRUMENTATION
        )
    endif()
endforeach()

add_custom_target(
    build_unittests
    DEPENDS
        ${ALL_TESTS_BUILD}
)

add_custom_target(
    run_unittests
    DEPENDS
        ${ALL_TESTS_RUN}
)

set_directory_properties(PROPERTIES
    IN_BUILD_TESTS "${ALL_TESTS_RUN}"
)

if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
    enable_coverage_report(COVERAGE_REPORT_FORMATS html
        ROOT_DIRECTORY ${LIBCYPHAL_ROOT}
        OUT_REPORT_INDICIES LOCAL_COVERAGE_REPORT_INDICIES
    )
endif()
