#!/usr/bin/env python3
#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#
"""
    Command-line helper for running verification builds.

    emrainey says this script should be replaced with conan profiles. I'll look
    into this but for now "it's better than a bash script."

"""

import argparse
import functools
import logging
import os
import pathlib
import re
import shutil
import subprocess
import sys
import textwrap
import typing

import xml.etree.ElementTree as ET

# +---------------------------------------------------------------------------+


def _make_parser() -> argparse.ArgumentParser:

    epilog = textwrap.dedent(
        """

        **Example Usage**::

            ./verify.py docs

    """
    )

    parser = argparse.ArgumentParser(
        description="CMake command-line helper for running cetlvast suites.",
        epilog=epilog,
        formatter_class=argparse.RawTextHelpFormatter,
    )

    parser.add_argument(
        "-v",
        "--verbose",
        action="count",
        default=0,
        help=textwrap.dedent(
            """
        Used to form -DCMAKE_MESSAGE_LOG_LEVEL and other options passed into
        cmake as well as the verbosity of this script.

        #      | cmake log-level | verify.py | cmake options
        ----------------------------------------------------
        (none) : NOTICE          : warning   :
             1 : STATUS          : warning   : --warn-uninitialized
             2 : VERBOSE         : info      : --warn-uninitialized
             3 : DEBUG           : debug     : --warn-uninitialized
             4 : TRACE           : debug     : --warn-uninitialized
             5 : TRACE           : debug     : --trace --warn-uninitialized
            6+ : TRACE           : debug     : --trace-expand --warn-uninitialized

    """[1:])
    )

    kit_args = parser.add_argument_group(
        title="CETLVaSt suites",
        description=textwrap.dedent(
            """
        Select the test suite to run and the toolchain to run it against.
    """[1:])
    )

    kit_args.add_argument(
        "suite",
        choices=["none", "clean", "unittest", "compile", "lint", "docs", "ontarget"],
        default="none",
        help=textwrap.dedent(
            """
        Used to form -DCETLVAST_TEST_SUITE value

        The five cetlvast suites are:
        -----------------------------------------------------------------------

            unit tests      : googletest/googlemock tests compiled and executed
                              on the local system. these tests verify the core
                              interface contracts of CETL types are not violated
                              including the behavioural contracts, where
                              defined. These also provide some level of
                              verification of compatibility with different tool
                              chains.
            compile tests   : A specalized suite of unit tests that run in the
                              compiler. These verify that certain invariants are
                              properly protected by static assertions.
            lint/analysis   : Runs various static analysis tools against the
                              cetl types.
            documentation   : While not typically considered a test, this target
                              builds the doxygen documentation for cetlvast
                              which verifies that the types are all properly
                              documented. Furthermore, this builds and executes
                              any example code included in the docs to ensure
                              the code is correct.
            on-target       : This is future work. The suite will build one or
                              more firmware images for verifying the performance
                              of CETL on a select one or two embedded
                              processors.

        Psedo-suites:
            none            : No test suite.

    """[1:])
    )

    kit_args.add_argument(
        "-tc",
        "--toolchain",
        choices=["gcc", "clang"],
        default="clang",
        help=textwrap.dedent(
            """

        Used to form -DCMAKE_TOOLCHAIN_FILE value

        This selects the toolchain description cetlvast will tell Cmake to use.

    """[1:])
    )

    variant_args = parser.add_argument_group(
        title="build variants",
        description=textwrap.dedent(
            """
        Arguments the modify build parameters.
    """[1:])
    )

    variant_args.add_argument(
        "-bf",
        "--build-flavor",
        choices=["Debug", "Release"],
        default="Debug",
        help=textwrap.dedent(
            """
        Sets -DCMAKE_BUILD_TYPE value

        Debug   : builds will be lightly optimized or not optimized. Debug
                  symbols will be included.
        Release : builds will be reasonably optimized.

    """[1:])
    )

    variant_args.add_argument(
        "--coverage",
        choices=["sonarqube", "html"],
        help=textwrap.dedent(
            """
        Enables instrumentation of code and selects coverage report format.

        -DCETLVAST_ENABLE_COVERAGE:BOOL=ON

        -DCETLVAST_COVERAGE_REPORT_FORMAT:STRING=<value>
        -----------------------------------------------------------------------

        html                  : Generages a human-readable HTML report.
        sonarqube             : Generates an XML file compatible with sonarqube.

    """[1:])
    )

    variant_args.add_argument(
        "-cda",
        "--asserts",
        action="store_true",
        help=textwrap.dedent(
            """
        Sets -DCETL_ENABLE_DEBUG_ASSERT:BOOL=ON

        Enables CETL debug asserts. Also forces the build flavor to be Debug.

    """[1:])
    )

    variant_args.add_argument(
        "-std",
        "--cpp-standard",
        choices=["base", "intermediate", "target"],
        default="target",
        help=textwrap.dedent(
            """

        Sets -DCETLVAST_CPP_STANDARD value
        -----------------------------------------------------------------------

        base (C++14)          : Use the C++ 14 standard which is the base
                                standard for CETL 1.0. This allows testing of
                                CETL as a polyfill library for C++17 and 20.
        intermediate (C++17)  : Use the C++ 17 standard. This enables testing of
                                CETL as a polyfill library for C++20 and enables
                                A/B testing of CETL against any C++17 types
                                it supports.
        target (C++20)        : Use the C++20 standard, the target support level
                                for CETL 1.0. This enables A/B testing of CETL
                                to ensure forwards compatibility.

    """[1:])
    )

    action_args = parser.add_argument_group(
        title="action modifiers",
        description=textwrap.dedent(
            """
        Arguments that change the actions taken by this script.
    """[1:])
    )

    action_args.add_argument(
        "-f",
        "--force",
        action="store_true",
        help=textwrap.dedent(
            """
        Force recreation of verification directory if it already exists.

        ** WARNING ** This will delete the cmake build directory!

    """[1:])
    )

    action_args.add_argument(
        "--version",
        action="store_true",
        help=textwrap.dedent(
            """
        Emits the current version. Use "none" suite to simply emit the version
        and exit:

            export CETL_VERSION=$(./verify.py --version none)

    """[1:])
    )

    action_args.add_argument(
        "-ls",
        "--list",
        choices=["builddir", "extdir", "cppstd", "covri", "covrd", "tests"],
        help=textwrap.dedent(
            """

        Lists various internal values to allow integration with CI or other
        build scripts without hardcoding these values.

        -ls/--list values
        -----------------------------------------------------------------------

        builddir    Emits a relative path to a build directory. Use with "none"
                    suite to emit the root build path and exit. For example:

                        pushd $(./verify.py -ls builddir none)
                        ninja -t commands
                        popd

                    Use with a suite to emite a path to the suite's build
                    output. For example:

                        open "$(./verify.py -ls buildir docs)/html/index.html"

        extdir      Emits a relative path to the external test dependencies
                    directory. Use with "none" suite to simply emit this path
                    and exit. For example:

                      - name: setup environment
                        run: |
                            echo "CETLVAST_EXT_PATH=$(./cetlvast/verify.py -cd ./cetlvast -ls extdir none)" >> $GITHUB_ENV

        cppstd      Emits an integer value for the cpp standard defined by
                    default or by a --cpp-standard argument provided to this
                    script. For example:

                        echo "-std=c++$(./verify.py --cpp-standard base -ls cppstd docs)"

        covri       If --coverage is set this will return a path to a report index file.

                        open $(./verify.py --coverage html -ls covri unittest)

        covrd       If --coverage is set this will return a path to a file or folder that
                    is/contains the coverage report.

                        tar -vzcf report.gz $(./verify.py --coverage html -ls covrd unittest)

        tests       If --generate-test-report is set this will return a path to the output file.

                        ./verify.py -gtr tests.xml -ls tests unittest | xargs cat

        All ls actions happen before the build directory action so -rm will be
        ignored.

    """[1:])
    )

    action_args.add_argument(
        "-gtr",
        "--generate-test-report",
        type=pathlib.Path,
        metavar="{output_file}",
        help=textwrap.dedent(
            """
        Writes a test execution report in the sonarqube generic test execution
        format after test execution.

            ./verify.py --generate-test-report path/to/write/sonarqube.xml unittest

    """[1:])
    )

    action_args.add_argument(
        "--builddir-only",
        action="store_true",
        help=textwrap.dedent(
            """
        Handle -rm and -f arguments but do not configure, build, or run tests.
        Use with none to perform a nuclear-clean operation:

            alias cv_superclean="./verify.py -rm -f --builddir-only none"

    """[1:])
    )

    action_args.add_argument(
        "-c",
        "--configure-only",
        action="store_true",
        help=textwrap.dedent(
            """
        Configure but do not build.
    """[1:])
    )

    action_args.add_argument(
        "-b",
        "--build-only",
        action="store_true",
        help=textwrap.dedent(
            """
        Try to build without configuring. Do not try to run tests.
    """[1:])
    )

    action_args.add_argument(
        "-t",
        "--test-only",
        action="store_true",
        help=textwrap.dedent(
            """
        Only try to run tests. Don't configure or build.
    """[1:])
    )

    action_args.add_argument(
        "-co",
        "--clean-only",
        action="store_true",
        help=textwrap.dedent(
            """
        If specified, this will only run the cmake generated clean target for
        the provided suite and configuration then exit.

            ./verify.py --clean-only unittest

    """[1:])
    )

    action_args.add_argument(
        "--dry-run",
        action="store_true",
        help=textwrap.dedent(
            """
        Don't actually do anything. Just log what this script would have done.
        Combine with --verbose to ensure you actually see the script's log
        output.
    """[1:])
    )

    action_args.add_argument(
        "-rm",
        "--remove-first",
        action="store_true",
        help=textwrap.dedent(
            """
        If specified, any existing build directory will be deleted first. Use
        -f to skip the user prompt.

        Note: This only applies to the configure step. If you do a build-only
        this argument has no effect.
    """[1:])
    )

    other_args = parser.add_argument_group(
        title="other options",
        description=textwrap.dedent(
            """
        Additional stuff you probably can ignore.
    """[1:])
    )

    other_args.add_argument(
        "--build-dir-name",
        default="verifypy",
        help=textwrap.dedent(
            """
        This script always uses [build_{build-dir-name}] as the name of the
        top-level directory it creates and under which it does its work. This
        option lets you change the {build-dir-name} part of that file name.

    """[1:])
    )

    other_args.add_argument(
        "-cd",
        "--cetlvast-dir",
        default=pathlib.Path.cwd(),
        type=pathlib.Path,
        help=textwrap.dedent(
            """
        By default this script uses the current-working directory as the root
        for CETLVaSt. Use this option to specify a different root directory when
        running the script.

    """[1:])
    )

    other_args.add_argument(
        "--force-ninja",
        action="store_true",
        help=textwrap.dedent(
            """

        -DCMAKE_GENERATOR=Ninja

        Form an argument requireing cmake use the Ninja build system instead of
        the default for the current system which can be make.

    """[1:])
    )


    return parser


# +---------------------------------------------------------------------------+


def _junit_to_sonarqube_generic_execution_format(junit_report: pathlib.Path, test_executions: ET.Element) -> None:
    """Append junit testsuite data to sonarqube testExecutions data.

    Input Format: http://google.github.io/googletest/advanced.html#generating-an-xml-report
    Output Format: https://docs.sonarqube.org/8.9/analyzing-source-code/generic-test-data/#generic-execution

    TODO: move this out into a standalone Python file (or, hell, even a Pypi package) and hook it up properly
          to cmake as a target.

    """
    sq_files: typing.Dict[str, ET.Element] = dict()
    junit_xml = ET.parse(junit_report)

    testsuite_or_testsuites = junit_xml.getroot()

    if testsuite_or_testsuites.tag == "testsuite":
        testsuites: typing.Iterable[ET.Element] = [testsuite_or_testsuites]
    else:
        testsuites = testsuite_or_testsuites.findall("testsuite")

    for testsuite in testsuites:
        logging.info("junit2sonarqube: parsing junit testsuite: {} name=\"{}\" tests=\"{}\""
            .format(testsuite.tag,
                    testsuite.get("name"),
                    testsuite.get("tests")))

        for testcase in testsuite:

            testcase_file = testcase.get("file")
            if testcase_file is not None:
                quirksmode = "gtest"
            else:
                quirksmode = "ctest"
                testcase_file = testcase.get("classname")
                if testcase_file is None:
                    logging.warn("junit2sonarqube: Unknown tag {} (skipping)".format(testcase.tag))
                    continue

            testcase_name: str = testcase.get("name", "")
            if (type_param := testcase.get("type_param", None)) != None:
                # mypy is not able to parse assignment expressions, aparently.
                testcase_name = testcase_name + " " + type_param # type: ignore

            logging.debug("junit2sonarqube: found testcase \"{}\" (quirks={})".format(testcase_name, quirksmode))

            try:
                sq_file = sq_files[testcase_file]
            except KeyError:
                sq_file = ET.Element("file", attrib={"path": testcase_file})
                test_executions.append(sq_file)
                sq_files[testcase_file] = sq_file

            sq_testcase_attrib: typing.Dict[str, str] = dict()
            sq_testcase_attrib["name"] = testcase_name
            test_duration = float(testcase.get("time", 0.0))
            sq_testcase_attrib["duration"] = str(test_duration)

            sq_test_case = ET.Element("testCase", attrib=sq_testcase_attrib)
            sq_file.append(sq_test_case)

            skipped: typing.Optional[ET.Element] = testcase.find("skipped")
            if skipped is not None:
                sq_skipped = ET.Element("skipped", attrib={"message": skipped.get("message", "").rstrip()})
                sq_skipped.text = skipped.text.rstrip()  # type: ignore
                sq_test_case.append(sq_skipped)
            failures = testcase.findall("failure")
            if failures is not None and len(failures) > 0:
                sq_failure = ET.Element("failure", attrib={"message": "failed"})
                sq_test_case.append(sq_failure)
                sq_failure_text = []
                for failure in failures:
                    for failure_line in failure.get("message", "").split("\n"):
                        sq_failure_text.append(failure_line)
                sq_failure.set("message", sq_failure_text[0])
                if len(sq_failure_text) > 1:
                    sq_failure.text = "\n".join(sq_failure_text[1:])


# +---------------------------------------------------------------------------+


def _is_pseudo_suite(suite_name: str) -> bool:
    return (suite_name in ["none"])


# +---------------------------------------------------------------------------+


def _suite_dir(args: argparse.Namespace, cmake_dir: pathlib.Path) -> pathlib.Path:
    if _is_pseudo_suite(args.suite):
        raise RuntimeError("Cannot create path to a psedo-suite.")
    return cmake_dir / "cetlvast" / "suites" / args.suite


# +---------------------------------------------------------------------------+


def _cpp_standard_arg_to_number(args: argparse.Namespace, ) -> int:
    if args.cpp_standard == "base":
        return 14
    elif args.cpp_standard == "intermediate":
        return 17
    elif args.cpp_standard == "target":
        return 20
    else:
        raise RuntimeError("internal error: illegal cpp-standard choice got through? ({})".format(args.cpp_standard))


# +---------------------------------------------------------------------------+


def _to_cmake_logging_level(verbose: int) -> str:
    if verbose == 1:
        cmake_logging_level = "STATUS"
    elif verbose == 2:
        cmake_logging_level = "VERBOSE"
    elif verbose == 3:
        cmake_logging_level = "DEBUG"
    elif verbose > 3:
        cmake_logging_level = "TRACE"
    else:
        cmake_logging_level = "NOTICE"

    return cmake_logging_level


# +---------------------------------------------------------------------------+


def _cmake_run(
    cmake_args: typing.List[str],
    cmake_dir: pathlib.Path,
    verbose: int,
    dry_run: bool,
    env: typing.Optional[typing.Dict] = None,
) -> int:
    """
    Simple wrapper around cmake execution logic
    """
    logging.info(
        textwrap.dedent(
            """
    *****************************************************************
    About to run command: {}
    in directory        : {}
    *****************************************************************
    """
        ).format(" ".join(cmake_args), str(cmake_dir))
    )

    copy_of_env: typing.Dict = {}
    copy_of_env.update(os.environ)
    if env is not None:
        copy_of_env.update(env)

    if verbose > 1:
        logging.debug("        *****************************************************************")
        logging.debug("        Using Environment:")
        for key, value in copy_of_env.items():
            overridden = key in env if env is not None else False
            logging.debug("            {} = {}{}".format(key, value, (" (override)" if overridden else "")))
        logging.debug("        *****************************************************************\n")

    if not dry_run:
        return subprocess.run(cmake_args, cwd=cmake_dir, env=copy_of_env).returncode
    else:
        return 0


# +---------------------------------------------------------------------------+


def _remove_build_dir_action(args: argparse.Namespace, cmake_dir: pathlib.Path) -> int:
    """
    Handle all the logic, user input, logging, and file-system operations needed to
    remove the cmake build directory ahead of invoking cmake.
    """
    if args.remove_first and cmake_dir.exists():
        okay_to_remove = False
        if not args.force:
            response = input("Are you sure you want to delete {}? [y/N]:".format(cmake_dir))
            if (len(response) == 1 and response.lower() == "y") or (len(response) == 3 and response.lower() == "yes"):
                okay_to_remove = True
        else:
            okay_to_remove = True

        if okay_to_remove:
            if not args.dry_run:
                logging.info("Removing directory {}".format(cmake_dir))
                shutil.rmtree(cmake_dir)
            else:
                logging.info("Is dry-run. Would have removed directory {}".format(cmake_dir))
        else:
            raise RuntimeError(
                """
                Build directory {} already exists, -rm or --remove-first was specified,
                and permission was not granted to delete it. We cannot continue. Either
                allow re-use of this build directory or allow deletion. (use -f flag to
                skip user prompts).""".lstrip().format(
                    cmake_dir
                )
            )
        return 0
    else:
        return 1


# +---------------------------------------------------------------------------+


def _create_build_dir_action(args: argparse.Namespace, cmake_dir: pathlib.Path) -> None:
    """
    Handle all the logic, user input, logging, and file-system operations needed to
    create the cmake build directory ahead of invoking cmake.
    """
    if not cmake_dir.exists():
        if not args.dry_run:
            logging.info("Creating build directory at {}".format(cmake_dir))
            cmake_dir.mkdir()
        else:
            logging.info("Dry run: Would have created build directory at {}".format(cmake_dir))
    else:
        logging.info("Using existing build directory at {}".format(cmake_dir))


# +---------------------------------------------------------------------------+


def _cmake_configure(args: argparse.Namespace, cmake_args: typing.List[str], cmake_dir: pathlib.Path, skip: bool = False) -> int:
    """
    Format and execute cmake configure command. This also include the cmake build directory (re)creation
    logic.
    """

    if args.build_only or args.test_only:
        return 0

    cmake_configure_args = cmake_args.copy()

    cmake_configure_args.append("-DCMAKE_MESSAGE_LOG_LEVEL:STRING={}".format(_to_cmake_logging_level(args.verbose)))

    if args.verbose >= 1:
        cmake_configure_args.append("--warn-uninitialized")
        if args.verbose == 5:
            cmake_configure_args.append("--trace")
        elif args.verbose >= 6:
            cmake_configure_args.append("--trace-expand")

    flag_set_dir = pathlib.Path("cmake") / pathlib.Path("compiler_flag_sets")
    flagset_file = (flag_set_dir / pathlib.Path("native")).with_suffix(_cmake_configure.cmake_suffix)

    cmake_configure_args.append("-DCETLVAST_FLAG_SET={}".format(str(flagset_file)))

    if not _is_pseudo_suite(args.suite):
        test_suite_dir = pathlib.Path("cmake") / pathlib.Path("suites")
        test_suite_dir = (test_suite_dir / pathlib.Path(args.suite)).with_suffix(_cmake_configure.cmake_suffix)
        cmake_configure_args.append("-DCETLVAST_TEST_SUITE={}".format(str(test_suite_dir)))
    if args.toolchain != "none":
        toolchain = pathlib.Path("cmake") / pathlib.Path("toolchains")
        if args.toolchain == "clang":
            toolchain_file = toolchain / pathlib.Path("clang-native").with_suffix(_cmake_configure.cmake_suffix)
        else:
            toolchain_file = toolchain / pathlib.Path("gcc-native").with_suffix(_cmake_configure.cmake_suffix)

        cmake_configure_args.append("-DCMAKE_TOOLCHAIN_FILE={}".format(str(toolchain_file)))

    if args.coverage is not None:
        cmake_configure_args.append("-DCETLVAST_ENABLE_COVERAGE:BOOL=ON")
        cmake_configure_args.append("-DCETLVAST_COVERAGE_REPORT_FORMAT:STRING={}".format(args.coverage))
    else:
        cmake_configure_args.append("-DCETLVAST_ENABLE_COVERAGE:BOOL=OFF")

    if args.asserts:
        if args.build_flavor != "Debug":
            logging.warning("-cda/--asserts forces the build to be Debug. Ignoring -bf/--build-flavor {}".format(args.build_flavor))
        cmake_configure_args.append("-DCMAKE_BUILD_TYPE=Debug")
        cmake_configure_args.append("-DCETL_ENABLE_DEBUG_ASSERT:BOOL=ON")
    else:
        cmake_configure_args.append("-DCMAKE_BUILD_TYPE={}".format(args.build_flavor))
        cmake_configure_args.append("-DCETL_ENABLE_DEBUG_ASSERT:BOOL=OFF")

    cmake_configure_args.append("-DCETLVAST_CPP_STANDARD={}".format(_cpp_standard_arg_to_number(args)))

    if args.force_ninja:
        cmake_configure_args.append("-DCMAKE_GENERATOR=Ninja")

    cmake_configure_args.append("..")

    if skip:
        return 0
    else:
        return _cmake_run(cmake_configure_args, cmake_dir, args.verbose, args.dry_run)

_cmake_configure.cmake_suffix = ".cmake"

# +---------------------------------------------------------------------------+


def _cmake_build(args: argparse.Namespace, cmake_args: typing.List[str], cmake_dir: pathlib.Path) -> int:
    """
    Format and execute cmake build command. This method assumes that the cmake_dir is already properly
    configured.
    """
    if not args.configure_only and not args.test_only:
        cmake_build_args = cmake_args.copy()

        cmake_build_args += ["--build", ".", "--target"]

        if _is_pseudo_suite(args.suite):
            logging.debug("no concrete test suite specified. Nothing to do.")
            return 0
        elif args.suite == "lint":
            logging.debug("lint target doesn't currently have a build step")
            return 0
        elif args.suite == "ontarget":
            logging.warning("ontarget tests not implemented yet!")
            return -1
        elif args.clean_only:
            cmake_build_args.append("clean")
        else:
            cmake_build_args.append("build_all")

        return _cmake_run(cmake_build_args, cmake_dir, args.verbose, args.dry_run)

    return 0


# +---------------------------------------------------------------------------+


def _cmake_test(args: argparse.Namespace, cmake_args: typing.List[str], cmake_dir: pathlib.Path) -> int:
    """
    Format and execute cmake test command. This method assumes that the cmake_dir is already properly
    configured.
    """
    if not args.configure_only and not args.build_only:

        cmake_test_args = cmake_args.copy()

        cmake_test_args += ["--build", ".", "--target"]

        if args.suite == "ontarget":
            logging.warning("ontarget tests not implemented yet!")
            return -1
        elif _is_pseudo_suite(args.suite):
            logging.debug("No test suite specified. Nothing to do.")
            return 0
        else:
            cmake_test_args.append("suite_all")

        return _cmake_run(cmake_test_args, cmake_dir, args.verbose, args.dry_run)

    return 0


# +---------------------------------------------------------------------------+


def _cmake_ctest(args: argparse.Namespace, _: typing.List[str], cmake_dir: pathlib.Path) -> int:
    """
    run ctest
    """
    if not args.configure_only and not args.build_only:

        if args.suite == "compile":
            # we use ctest to run the compile tests so we take a different
            # branch here.
            report_path = pathlib.Path.cwd().joinpath(_suite_dir(args, cmake_dir) / "ctest.xml")
            ctest_run = ["ctest", "-DCTEST_FULL_OUTPUT", "--output-junit", str(report_path)]
            if not args.dry_run:
                logging.debug("about to run {}".format(str(ctest_run)))
                return subprocess.run(ctest_run, cwd=cmake_dir).returncode
            else:
                logging.info("Is dry-run. Would have run ctest: {}".format(str(ctest_run)))
                return 0
        else:
            logging.debug("No ctest action defined for {} test suite.".format(args.suite))

    return 0


# +---------------------------------------------------------------------------+


def _create_build_dir_name(args: argparse.Namespace) -> str:
    return "build_{}".format(args.build_dir_name)


# +---------------------------------------------------------------------------+


@functools.cache
def _get_version_string(gitdir: pathlib.Path) -> typing.Tuple[int, int, int, str]:
    git_output = subprocess.run(["git", "describe", "--abbrev=0", "--tags"], cwd=gitdir, capture_output=True, text=True).stdout
    if (match_obj := re.match(r"^v(\d+)\.(\d+)\.(\d+)[-_]?(\w*)", git_output)) is not None:
        _version_string = (int(match_obj.group(1)),
                           int(match_obj.group(2)),
                           int(match_obj.group(3)),
                           qualifier if (qualifier:=match_obj.group(4)) else "")
    else:
       _version_string = (0,0,0,"")

    return _version_string


# +---------------------------------------------------------------------------+


def _handle_special_actions(args: argparse.Namespace, cmake_dir: pathlib.Path, gitdir: pathlib.Path) -> int:

    if args.version:
        sys.stdout.write("{}.{}.{}{}".format(*_get_version_string(gitdir)))

    elif args.list is not None:
        if args.list == "builddir":
            if _is_pseudo_suite(args.suite):
                sys.stdout.write(str(cmake_dir))
            else:
                sys.stdout.write(str(_suite_dir(args, cmake_dir)))

        elif args.list == "extdir":
            sys.stdout.write(str(cmake_dir.parent / pathlib.Path(cmake_dir.stem + "_ext")))
        elif args.list == "cppstd":
            sys.stdout.write("{}".format(_cpp_standard_arg_to_number(args)))
        elif args.list == "tests":
            if args.generate_test_report is None:
                logging.error("Cannot list the test report unless --generate-test-report is also specified.")
                return -1
            sys.stdout.write(str(_suite_dir(args, cmake_dir) / args.generate_test_report))
        elif (is_covri := (args.list == "covri")) or args.list == "covrd":
            if args.coverage is None:
                raise RuntimeError("cannot list coverage output unless --coverage is specified.")
            elif not _is_pseudo_suite(args.suite):
                if args.coverage == "html":
                    html_dir = _suite_dir(args, cmake_dir) / "gcovr_html"
                    sys.stdout.write( str(html_dir / "coverage.html") if is_covri else str(html_dir) )
                elif args.coverage == "sonarqube":
                    suitedir = _suite_dir(args, cmake_dir)
                    sys.stdout.write( str(suitedir / "coverage.xml") )

        else:
            raise RuntimeError("invalid ls value {} got through argparse?".format(args.list))
    return 0


# +---------------------------------------------------------------------------+


def _handle_generate_test_report(args: argparse.Namespace, cmake_dir: pathlib.Path, test_result: int) -> int:
    if (output_file := args.generate_test_report) is None:
        return 0

    output_path = pathlib.Path.cwd().joinpath(_suite_dir(args, cmake_dir) / output_file)
    test_executions = ET.Element("testExecutions", attrib={"version": "1"})
    sq_report = ET.ElementTree(test_executions)

    for gtest_report in _suite_dir(args, cmake_dir).glob("*-gtest.xml"):
        logging.debug("Found gtest report {}. Will combine into sonarqube report.".format(gtest_report))
        _junit_to_sonarqube_generic_execution_format(gtest_report, sq_report.getroot())

    for ctest_report in _suite_dir(args, cmake_dir).glob("*ctest.xml"):
        logging.debug("Found ctest report {}. Will combine into sonarqube report.".format(ctest_report))
        _junit_to_sonarqube_generic_execution_format(ctest_report, sq_report.getroot())

    if args.dry_run:
        logging.debug("Would have written a test report for {} files to {}".format(len(test_executions.findall("file")), output_path))
    else:
        logging.debug("About to write a test report for {} files to {}".format(len(test_executions.findall("file")), output_path))
        ET.indent(sq_report)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        sq_report.write(output_path, encoding="UTF-8")
    return test_result


# +---------------------------------------------------------------------------+


def main() -> int:
    """
    Main method to execute when this package/script is invoked as a command.
    """
    args = _make_parser().parse_args()

    verification_dir = args.cetlvast_dir
    cmake_dir = verification_dir / pathlib.Path(_create_build_dir_name(args))
    cmake_args = ["cmake"]

    logging_level = logging.WARN

    if args.verbose == 2:
        logging_level = logging.INFO
    elif args.verbose >= 3:
        logging_level = logging.DEBUG

    logging.basicConfig(format="%(levelname)s: %(message)s", level=logging_level)

    logging.info(
        textwrap.dedent(
            """

    *****************************************************************
    Commandline Arguments to {}:

    {}

    For verify version {}
    *****************************************************************

    """
        ).format(os.path.basename(__file__), str(args), _get_version_string(verification_dir))
    )

    special_action_result = _handle_special_actions(args, cmake_dir, verification_dir)

    if special_action_result != 0:
        return special_action_result
    elif args.list is not None:
        return 0

    remove_build_dir_result = _remove_build_dir_action(args, cmake_dir)
    if (0 == remove_build_dir_result) and args.clean_only:
        logging.debug("Since we deleted the build directory there's no point in running the clean target.")
        return 0
    elif remove_build_dir_result < 0 and remove_build_dir_result > 1: # 1 means there wasn't a request to remove the build directory.
        return remove_build_dir_result

    if args.builddir_only or _is_pseudo_suite(args.suite):
        return 0

    _create_build_dir_action(args, cmake_dir)

    configure_result = _cmake_configure(args, cmake_args, cmake_dir, args.builddir_only | args.clean_only)

    if configure_result != 0:
        return configure_result
    elif args.configure_only:
        return 0

    build_result = _cmake_build(args, cmake_args, cmake_dir)

    if build_result != 0:
        return build_result
    elif args.build_only or args.clean_only:
        return 0

    if not args.configure_only and not args.build_only and not args.clean_only:
        test_result = _cmake_test(args, cmake_args, cmake_dir)

        if test_result == 0:
            test_result = _cmake_ctest(args, cmake_args, cmake_dir)

        return _handle_generate_test_report(args, cmake_dir, test_result)

    raise RuntimeError("Internal logic error: only_do_x flags resulted in no action.")


# +---------------------------------------------------------------------------+


if __name__ == "__main__":
    sys.exit(main())
