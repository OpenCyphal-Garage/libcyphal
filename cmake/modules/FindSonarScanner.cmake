#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

#
# Fins sonar-scanner and define a scan target
#

find_program(SONAR_SCANNER sonar-scanner)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(SonarScanner
    REQUIRED_VARS SONAR_SCANNER
)

find_package(Git)

set(SONARSCANNER_SONARCLOUD_URL "https://sonarcloud.io")
set(SONARSCANNER_DEFAULT_SOURCE_ENCODING "UTF-8")

#
# :function: define_sonar_scan_target
# Create a target that runs sonar-scanner.
#
# N.B.: If you are scanning a header-only library this target will refuse to index the headers unless you have at least
# one source file. This is a (dumb) limitation of sonarscanner. To work around this simply add a dummy source file
# (e.g. a .cpp file with nothing in it) to your project but be sure it shows up in the compile_commands.json file.
#
# :param path:          ROOT_DIRECTORY      - The root directory of the project. Default is "${CMAKE_SOURCE_DIR}"
# :param string:        ORGANIZATION        - The organization name on SonarCloud. Default is "opencyphal"
# :param string:        PROJECT_KEY         - The unique key of the project on SonarCloud.
# :param string:        PROJECT_NAME        - The name of the project on SonarCloud.
# :param string:        CPP_VERSION         - C++ version number (Just the number. e.g. 14, 17, or 20).
# :param string:        PROJECT_VERSION     - The version of the project on SonarCloud. Default is "1.0"
# :param list[target]:  DEPENDS             - A list of targets to depend on.
# :param path:          COMPILE_COMMANDS    - The path to the compile_commands.json file. Default is
#                                             "build/comile_commands.json"
# :param list[path]:    SOURCES             - The source directories to scan.
# :param list[path]:    TESTS               - The test directories to scan.
# :param glob:          TEST_EXCLUSIONS     - A glob pattern to define exclusions from the test report.
# :param glob:          TEST_INCLUSIONS     - A glob pattern to define inclusions from the test report.
# :param list[path]:    COVERAGE_REPORTS    - A list of coverage reports to use.
# :param list[path]:    TEST_REPORTS        - A list of test reports to use.
# :param globpattern:   EXCLUDE_COVERAGE    - A glob pattern to define exclusions from the coverage report.
# :param globpattern:   EXCLUDE_CPD         - A glob pattern to define exclusions from the code duplication report.
# :param string:        BRANCH_NAME         - The name of the branch for ROOT_DIRECTORY. Default uses git to find the
#                                             current branch.
function (define_sonar_cloud_scan_target_for_c_cpp)
    #+-[input]----------------------------------------------------------------+
    set(options "")
    set(singleValueArgs
            ROOT_DIRECTORY
            ORGANIZATION
            PROJECT_KEY
            PROJECT_NAME
            PROJECT_VERSION
            EXCLUDE_COVERAGE
            EXCLUDE_CPD
            TEST_EXCLUSIONS
            TEST_INCLUSIONS
            COMPILE_COMMANDS
            CPP_VERSION
            BRANCH_NAME)
    set(multiValueArgs DEPENDS SOURCES TESTS COVERAGE_REPORTS TEST_REPORTS)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    if(NOT ARG_ROOT_DIRECTORY)
        set(ARG_ROOT_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()

    if(NOT ARG_ORGANIZATION)
        set(ARG_ORGANIZATION "opencyphal")
    endif()

    if(NOT ARG_PROJECT_VERSION)
        set(ARG_PROJECT_VERSION "1.0")
    endif()

    if(NOT ARG_COMPILE_COMMANDS)
        set(ARG_COMPILE_COMMANDS "build/compile_commands.json")
    endif()

    if(NOT ARG_BRANCH_NAME)
        if(NOT Git_FOUND)
            message(WARNING "Git not found. Cannot determine branch name.")
            set(ARG_BRANCH_NAME "develop/local")
        else()
            execute_process(
                COMMAND git rev-parse --abbrev-ref HEAD
                WORKING_DIRECTORY ${ARG_ROOT_DIRECTORY}
                OUTPUT_VARIABLE ARG_BRANCH_NAME
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            message(STATUS "sonarscan will use branch name from git: ${ARG_BRANCH_NAME}")
        endif()
    endif()

    #+-[body]-----------------------------------------------------------------+

    list(JOIN ARG_SOURCES "," LOCAL_SOURCES)
    list(JOIN ARG_TESTS "," LOCAL_TESTS)
    list(JOIN ARG_COVERAGE_REPORTS "," LOCAL_COVERAGE_REPORTS)
    list(JOIN ARG_TEST_REPORTS "," LOCAL_TEST_REPORTS)

    add_custom_target(sonar-cloud-scan-${ARG_PROJECT_KEY}
        DEPENDS ${ARG_DEPENDS}
        USES_TERMINAL
        COMMAND ${SONAR_SCANNER} --debug
            --define sonar.organization=${ARG_ORGANIZATION}
            --define sonar.projectKey=${ARG_PROJECT_KEY}
            --define sonar.projectName=${ARG_PROJECT_NAME}
            --define sonar.projectVersion=${ARG_PROJECT_VERSION}
            --define sonar.sources=${LOCAL_SOURCES}
            --define sonar.tests=${LOCAL_TESTS}
            --define sonar.test.exclusions=${ARG_TEST_EXCLUSIONS}
            --define sonar.test.inclusions=${ARG_TEST_INCLUSIONS}
            --define sonar.sourceEncoding=${SONARSCANNER_DEFAULT_SOURCE_ENCODING}
            --define sonar.host.url=${SONARSCANNER_SONARCLOUD_URL}
            --define sonar.cfamily.ignoreHeaderComments=false
            --define sonar.coverage.exclusions=${ARG_EXCLUDE_COVERAGE}
            --define sonar.cpd.exclusions=${ARG_EXCLUDE_CPD}
            --define sonar.cfamily.compile-commands=${ARG_COMPILE_COMMANDS}
            --define sonar.cfamily.reportingCppStandardOverride=c++${ARG_CPP_VERSION}
            --define sonar.coverageReportPaths=${LOCAL_COVERAGE_REPORTS}
            --define sonar.testExecutionReportPaths=${LOCAL_TEST_REPORTS}
            --define sonar.branch.name=${ARG_BRANCH_NAME}
        WORKING_DIRECTORY ${ARG_ROOT_DIRECTORY}
        VERBATIM
        COMMENT "Running sonar-scanner using sonarcloud for ${ARG_PROJECT_KEY}")

    #+-[output]---------------------------------------------------------------+

endfunction(define_sonar_cloud_scan_target_for_c_cpp)
