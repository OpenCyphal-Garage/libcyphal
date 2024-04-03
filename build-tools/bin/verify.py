#!/usr/bin/env python3
#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#
"""
    CETLVaSt CMake command-line helper for running verification builds of opencyphal C/C++ projects.
"""
# cSpell: words levelname ontarget dlibcxx gitdir

import argparse
import functools
import logging
import os
import pathlib
import re
import sys
import subprocess
import textwrap
import typing


# +---------------------------------------------------------------------------+
# | UTILS
# +---------------------------------------------------------------------------+


@functools.lru_cache
def _get_version_number(gitdir: pathlib.Path) -> typing.Tuple[int, int, int, str]:
        git_output = subprocess.run(["git", "describe", "--abbrev=0", "--tags"], cwd=gitdir, capture_output=True, text=True).stdout
        match_obj = re.match(r"^v(\d+)\.(\d+)\.(\d+)[-_]?(\w*)", git_output)
        if match_obj is not None:
            qualifier = match_obj.group(4)
            _version_string = (int(match_obj.group(1)),
                            int(match_obj.group(2)),
                            int(match_obj.group(3)),
                            qualifier if qualifier else "")
        else:
            _version_string = (0,0,0,"")
        return _version_string



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


def _create_build_dir_name(args: argparse.Namespace) -> str:
    return "{}".format(args.build_dir_name)


# +---------------------------------------------------------------------------+
# | UTILS::DIRECTORIES
# +---------------------------------------------------------------------------+


def _root_dir(args: argparse.Namespace) -> pathlib.Path:
    return args.root_dir


# +---------------------------------------------------------------------------+


def _build_dir(args: argparse.Namespace) -> pathlib.Path:
    return _root_dir(args) / _create_build_dir_name(args)


# +---------------------------------------------------------------------------+


def _test_suite_dir(args: argparse.Namespace) -> pathlib.Path:
    return _root_dir(args) / args.test_suite_dir


# +---------------------------------------------------------------------------+


def _ext_dir(args: argparse.Namespace) -> pathlib.Path:
    return _root_dir(args) / args.ext_dir


# +---------------------------------------------------------------------------+
# | ARGPARSE
# +---------------------------------------------------------------------------+


class QualifiedAction:
    """
    Used to allow action names that have a qualified suffix like

    "clean" versus "clean-only" both being the same "clean" action but the
    latter means only the clean action instead of also the clean action.
    """

    ActionPattern = re.compile(r"^(?:(?P<prefix>clean)-)?(?P<action>configure|build|test|release|clean)(?:-(?P<only>only))?(?:-+(?P<suffix>\S+))?")

    def __init__(self, input: str):
        match_obj = self.ActionPattern.match(str(input))
        if match_obj is None:
            self._prefix = None
            self._name = ""
            self._suffix = None
            self._only = False
        else:
            self._prefix = match_obj.group("prefix")
            self._name = match_obj.group("action")
            self._suffix = match_obj.group("suffix")
            self._only = match_obj.group("only") is not None

    @staticmethod
    def __call__(cls, input: typing.Any) -> 'QualifiedAction':
        return cls(input)

    def __str__(self) -> str:
        return self._name

    def __eq__(self, other: typing.Any) -> bool:
        if (isinstance(other, QualifiedAction)):
            return (other._name == self._name) and (other._suffix == self._suffix)
        else:
            return str(other) == str(self)

    @property
    def prefix(self) -> typing.Optional[str]:
        return self._prefix

    @property
    def name(self) -> str:
        return self._name

    @property
    def suffix(self) -> typing.Optional[str]:
        return self._suffix

    @property
    def only(self) -> bool:
        return self._only


# +---------------------------------------------------------------------------+


def _make_parser() -> argparse.ArgumentParser:

    prolog =  textwrap.dedent(
        r"""
                                          _           _
   ___  _ __   ___ _ __   ___ _   _ _ __ | |__   __ _| |
  / _ \| '_ \ / _ | '_ \ / __| | | | '_ \| '_ \ / _` | |
 | (_) | |_) |  __| | | | (__| |_| | |_) | | | | (_| | |
  \___/| .__/ \___|_| |_|\___|\__, | .__/|_| |_|\__,_|_|
       |_|                    |___/|_|
-----------------------------------------------------------------------------------------
CMake command-line helper for running verification builds of opencyphal C/C++ projects.
    """
    )

    epilog = textwrap.dedent(
        """

        **Example Usage**::

            # default configure, build, test, and release
            ./build-tools/bin/verify.py

            # configure, build, and test of a Coverage build.
            ./build-tools/bin/verify.py test -bf Coverage

            # verbose clean, configure, build, of a Debug build
            # with runtime asserts enabled.
            ./build-tools/bin/verify.py clean-build -bf Debug -cda -vv

            # only run the example tests
            ./build-tools/bin/verify.py test-only-run_examples

    ---
    """
    )

    parser = argparse.ArgumentParser(
        description=prolog,
        epilog=epilog,
        formatter_class=argparse.RawTextHelpFormatter,
    )

    # --[COMMON ARGS]--------------------------------------
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
        (none) : NOTICE          : warning     :
             1 : STATUS          : warning     : --warn-uninitialized
             2 : VERBOSE         : info        : --warn-uninitialized
             3 : DEBUG           : debug       : --warn-uninitialized
             4 : TRACE           : debug + env : --warn-uninitialized
             5 : TRACE           : debug + env : --trace --warn-uninitialized
            6+ : TRACE           : debug + env : --trace-expand --warn-uninitialized

    """[1:])
    )

    parser.add_argument(
        "--version",
        action="store_true",
        help=textwrap.dedent(
            """
        Emits the current version.

            export CYPHAL_PROJECT_VERSION=$(./verify.py --version)

    """[1:])
    )

    # --[ACTIONS]------------------------------------------
    action_args = parser.add_argument_group(
        title="Actions",
        description=textwrap.dedent(
            """
        Select the actions to take.
    """[1:])
    )

    action_args.add_argument(
        "-tc",
        "--toolchain",
        choices=["gcc", "clang"],
        default="gcc",
        help=textwrap.dedent(
            """

        Used to form -DCMAKE_TOOLCHAIN_FILE value

        This selects the toolchain description to tell Cmake to use.

    """[1:])
    )

    action_args.add_argument(
        "action",
        choices=["clean", "configure", "build", "test", "release"],
        default="release",
        nargs="?",
        type=typing.cast(typing.Callable[[str], str], QualifiedAction),
        help=textwrap.dedent(
            """

        BUILD ACTIONS:
        -----------------------------------------------------------------------

            configure - Using cmake, create a build output folder, validate
                        that all build dependencies are available, and generate
                        build scripts.

            build -     Execute the generated build scripts producing build
                        artifacts.

            test -      Execute various tests and generate test reporting data
                        including coverage reports.

            release -   Package build artifacts and test reports for
                        publication.

        This script treats these as sequential steps, starting with configure,
        where specifying any one is will halt the build at this step.
        For example:

            ./bin/verify.py test

        ... would configure, then build, then run tests but would not execute
        the release phase.

        ONLY
        -----------------------------------------------------------------------
        Any action can be modified with the -only suffix to indicate that only
        that action should be performed. For example:

            ./bin/verify.py build-only

        CLEAN
        -----------------------------------------------------------------------
            clean -     Clean is special. If specified on its own then the
                        build only performs a clean action. If used with a
                        hyphenated second set then clean will run first
                        followed by the rules mentioned above. For example:

                            ./bin/verify.py clean-test

                        ... would first clean, then configure, then build, then
                        run the tests whereas:

                            ./bin/verify.py clean

                        ... would only run clean.

        SUFFIX
        -----------------------------------------------------------------------
        The suffix is used to modify the behavior of the action. For example:

            ./bin/verify.py test-build_examples

        ... would run the build target "build_examples" instead of the default
        for the test phase.

        -----------------------------------------------------------------------
        NOTE: the verify script does not expose the full capabilities of the
        cmake build it fronts. To run individual targets or introspect
        dependencies you should cd into the build directory and use cmake or
        ninja directly.

        For example:

            cd build
            ninja -t list
            ninja -t deps
            ...
        ---
    """[1:])
    )

    # --[VARIANTS]-----------------------------------------
    variant_args = parser.add_argument_group(
        title="build variants",
        description=textwrap.dedent(
            """
        Arguments that modify build parameters.
    """[1:])
    )

    variant_args.add_argument(
        "-bf",
        "--build-flavor",
        choices=["Debug", "Release", "Coverage"],
        default="Debug",
        help=textwrap.dedent(
            """
        Sets -DCMAKE_BUILD_TYPE value

        Coverage : builds will be un-optimized and code will be instrumented
                   to emit coverage data files when tests are run.
        Debug    : builds will be lightly optimized or not optimized. Debug
                   symbols will be included.
        Release  : builds will be reasonably optimized.

    """[1:])
    )

    variant_args.add_argument(
        "-cda",
        "--asserts",
        action="store_true",
        help=textwrap.dedent(
            """
        Enables various debug asserts.

          -DCETL_ENABLE_DEBUG_ASSERT:BOOL=ON
          -DLIBCYPHAL_INTROSPECTION_ENABLE_ASSERT:BOOL=ON
          -DLIBCXX_ENABLE_ASSERTIONS:BOOL=ON

    """[1:])
    )

    variant_args.add_argument(
        "-std",
        "--cpp-standard",
        default="14",
        help=textwrap.dedent(
            """

        The number part of a valid --std=c++{number} argument.

        Sets -DCETLVAST_CPP_STANDARD value

    """[1:])
    )

    exception_group = variant_args.add_mutually_exclusive_group()

    exception_group.add_argument(
        "-exceptions",
        "--exceptions",
        action="store_true",
        help=textwrap.dedent(
            """

        Compiles C++ with exceptions enabled.

    """[1:])
    )

    exception_group.add_argument(
        "-no-exceptions",
        "--no-exceptions",
        action="store_true",
        help=textwrap.dedent(
            """

        Compiles C++ with exceptions disabled.

        Sets -DCETLVAST_DISABLE_CPP_EXCEPTIONS:BOOL=ON

    """[1:])
    )

    # --[ACTION MOD]---------------------------------------
    action_mod_args = parser.add_argument_group(
        title="action modifiers",
        description=textwrap.dedent(
            """
        Arguments that change the actions taken by this script.
    """[1:])
    )

    action_mod_args.add_argument(
        "--dry-run",
        action="store_true",
        help=textwrap.dedent(
            """
        Don't actually do anything. Just log what this script would have done.
        Combine with --verbose to ensure you actually see the script's log
        output.
    """[1:])
    )

    action_mod_args.add_argument(
        "-ol",
        "--online",
        action="store_true",
        help=textwrap.dedent(
            """
        By default this script assumes no internet access. Specifying --online
        may enable additional steps like checking external dependencies or
        connecting to online linting services, etc.
    """[1:])
    )

    # --[MISC]---------------------------------------------
    other_args = parser.add_argument_group(
        title="other options",
        description=textwrap.dedent(
            """
        Additional stuff you probably can ignore.
    """[1:])
    )

    other_args.add_argument(
        "--build-dir-name",
        default="build",
        help=textwrap.dedent(
            """
        This script always uses {root-dir}/{build-dir-name} as the name of the
        build directory it creates. This option lets you change the
        {build-dir-name} part of that file name.

        See --root-dir argument for changing the root directory.


    """[1:])
    )

    other_args.add_argument(
        "-cd",
        "--root-dir",
        default=pathlib.Path.cwd(),
        type=pathlib.Path,
        help=textwrap.dedent(
            """
        By default this script uses the current-working directory as the
        project root. Use this option to specify a different root directory when
        running the script.

    """[1:])
    )

    other_args.add_argument(
        "-ts",
        "--test-suite-dir",
        default=pathlib.Path.cwd(),
        type=pathlib.Path,
        help=textwrap.dedent(
            """
        The name of the folder under the root-dir where the verification test suite's
        CMakeLists.txt can be found.

    """[1:])
    )

    other_args.add_argument(
        "--ext-dir",
        default=pathlib.Path("external"),
        type=pathlib.Path,
        help=textwrap.dedent(
            """
        The directory under which external cmake projects will be pulled and
        built.

    """[1:])
    )

    other_args.add_argument(
        "--dont-force-ninja",
        action="store_true",
        help=textwrap.dedent(
            """

        -DCMAKE_GENERATOR=Ninja is used by default. Set this to remove the
        preference and allow cmake to pick a default.

    """[1:])
    )


    return parser


# +---------------------------------------------------------------------------+
# | SUBPROCESS
# +---------------------------------------------------------------------------+


def _cmake_run(
    args: argparse.Namespace,
    cmake_args: typing.List[str],
    env: typing.Optional[typing.Dict] = None,
) -> int:
    """
    Simple wrapper around cmake execution logic to handle dry-run and verbose logging.
    """
    logging.info(
        textwrap.dedent(
            """
    *****************************************************************
    About to run command: {}
    in directory        : {}
    *****************************************************************
    """
        ).format(" ".join(cmake_args), str(_build_dir(args)))
    )

    copy_of_env: typing.Dict = {}
    copy_of_env.update(os.environ)
    if env is not None:
        copy_of_env.update(env)

    if args.verbose >= 4:
        logging.debug("        *****************************************************************")
        logging.debug("        Using Environment:")
        for key, value in copy_of_env.items():
            overridden = key in env if env is not None else False
            logging.debug("            {} = {}{}".format(key, value, (" (override)" if overridden else "")))
        logging.debug("        *****************************************************************\n")

    if not _build_dir(args).exists():
        if not args.dry_run:
            logging.error("Build directory {} does not exist. Did you forget to run configure?".format(_build_dir(args)))
            return 1
        else:
            return 0

    if not args.dry_run:
        return subprocess.run(cmake_args, cwd=_build_dir(args), env=copy_of_env).returncode
    else:
        return 0


# +---------------------------------------------------------------------------+


def _create_dir_action(args: argparse.Namespace, directory: pathlib.Path) -> int:
    """
    Handle all the logic, user input, logging, and file-system operations needed to
    create a directory.
    """
    if not directory.exists():
        if not args.dry_run:
            logging.info("Creating build directory at {}".format(str(directory)))
            directory.mkdir()
        else:
            logging.info("Dry run: Would have created build directory at {}".format(str(directory)))
    else:
        logging.info("Using existing build directory at {}".format(str(directory)))

    return 0


# +---------------------------------------------------------------------------+


def _create_build_dir_action(args: argparse.Namespace) -> int:
    """
    Handle all the logic, user input, logging, and file-system operations needed to
    create the cmake build directory ahead of invoking cmake.
    """
    return _create_dir_action(args, _build_dir(args))


# +---------------------------------------------------------------------------+


def _create_external_dir_action(args: argparse.Namespace) -> int:
    """
    Handle all the logic, user input, logging, and file-system operations needed to
    create the cmake external projects directory ahead of invoking cmake.
    """
    return _create_dir_action(args, _ext_dir(args))



# +---------------------------------------------------------------------------+
# | CMAKE ACTIONS
# +---------------------------------------------------------------------------+

_cmake_configure_cmake_suffix = ".cmake"


def _cmake_configure(args: argparse.Namespace, cmake_args: typing.List[str]) -> int:
    """
    Format and execute cmake configure command.
    """

    cmake_configure_args = cmake_args.copy()

    # --[VERSION NUMBER]-----------------------------------
    # set version number from git tag
    version = _get_version_number(_root_dir(args))
    version_string = "{}.{}.{}".format(version[0], version[1], version[2])
    cmake_configure_args.append("-DCYPHAL_PROJECT_VERSION={}".format(version_string))

    if not args.online:
        # see https://cmake.org/cmake/help/latest/module/FetchContent.html
        cmake_configure_args.append("-DFETCHCONTENT_FULLY_DISCONNECTED:BOOL=ON")
    else:
        cmake_configure_args.append("-DFETCHCONTENT_FULLY_DISCONNECTED:BOOL=OFF")

    # --[VERBOSITY]----------------------------------------
    cmake_configure_args.append("-DCMAKE_MESSAGE_LOG_LEVEL:STRING={}".format(_to_cmake_logging_level(args.verbose)))

    if args.verbose >= 1:
        cmake_configure_args.append("--warn-uninitialized")
        if args.verbose == 5:
            cmake_configure_args.append("--trace")
        elif args.verbose >= 6:
            cmake_configure_args.append("--trace-expand")

    # --[BUILD FLAVOR]-------------------------------------
    cmake_configure_args.append("-DCMAKE_BUILD_TYPE={}".format(args.build_flavor))

    # --[COMPILER FLAGS]-----------------------------------
    flag_set_dir = _test_suite_dir(args) / pathlib.Path("cmake") / pathlib.Path("compiler_flag_sets")
    flag_set_file = (flag_set_dir / pathlib.Path("default")).with_suffix(_cmake_configure_cmake_suffix)

    cmake_configure_args.append("-DCETLVAST_FLAG_SET={}".format(str(flag_set_file)))
    cmake_configure_args.append("-DCETLVAST_CPP_STANDARD={}".format(args.cpp_standard))

    # --[TOOL CHAIN]---------------------------------------
    if args.toolchain != "none":
        toolchain = _root_dir(args) / pathlib.Path(".devcontainer") / pathlib.Path("cmake") / pathlib.Path("toolchains")
        if args.toolchain == "clang":
            toolchain_file = toolchain / pathlib.Path("clang-native").with_suffix(_cmake_configure_cmake_suffix)
        else:
            toolchain_file = toolchain / pathlib.Path("gcc-native").with_suffix(_cmake_configure_cmake_suffix)

        cmake_configure_args.append("-DCMAKE_TOOLCHAIN_FILE={}".format(str(toolchain_file)))

    # --[DEBUG ASSERTIONS]---------------------------------
    if args.asserts:
        cmake_configure_args.append("-DCETL_ENABLE_DEBUG_ASSERT:BOOL=ON")
        cmake_configure_args.append("-DLIBCYPHAL_INTROSPECTION_ENABLE_ASSERT:BOOL=ON")
        cmake_configure_args.append("-DLIBCXX_ENABLE_ASSERTIONS:BOOL=ON")

    # --[EXCEPTIONS]---------------------------------------
    if args.no_exceptions:
        cmake_configure_args.append("-DCETLVAST_DISABLE_CPP_EXCEPTIONS:BOOL=ON")

    # --[BUILD TOOL]---------------------------------------
    if not args.dont_force_ninja:
        cmake_configure_args.append("-DCMAKE_GENERATOR=Ninja")

    # --[CMAKE IS GO!]-------------------------------------
    cmake_configure_args.append(str(_test_suite_dir(args)))

    return _cmake_run(args, cmake_configure_args)


# +---------------------------------------------------------------------------+


def _cmake_build(args: argparse.Namespace, cmake_args: typing.List[str], build_target: typing.Optional[str] = None) -> int:
    """
    Format and execute cmake build command. This method assumes that the build directory
    is already properly configured.
    """

    if build_target is None:
        build_target = "build"

    cmake_build_args = cmake_args.copy()

    cmake_build_args += ["--build", str(_build_dir(args)), "--target", build_target]

    return _cmake_run(args, cmake_build_args)


# +---------------------------------------------------------------------------+


def _cmake_test(args: argparse.Namespace, cmake_args: typing.List[str], test_target: typing.Optional[str] = None) -> int:
    """
    Format and execute cmake test command. This method assumes that the build directory
    is already properly configured.
    """
    if test_target is None:
        test_target = "unittest"

    return _cmake_build(args, cmake_args, test_target)


# +---------------------------------------------------------------------------+


def _cmake_release(args: argparse.Namespace, cmake_args: typing.List[str]) -> int:
    """
    Format and execute cmake release command. This method assumes that build step has already completed
    successfully.
    """
    return _cmake_build(args, cmake_args, "release")


# +---------------------------------------------------------------------------+


def _cmake_install(args: argparse.Namespace, cmake_args: typing.List[str]) -> int:
    """
    Format and execute cmake install command. This method assumes that build step has already completed
    successfully.
    """
    return _cmake_build(args, cmake_args, "install")


# +---------------------------------------------------------------------------+


def _cmake_clean(args: argparse.Namespace, cmake_args: typing.List[str]) -> int:
    """
    Format and execute cmake clean command. This method assumes that the configure step has already completed
    successfully.
    """
    if not _build_dir(args).exists():
       logging.info("Build directory {} does not exist. Nothing to clean.".format(_build_dir(args)))
       return 0

    return _cmake_build(args, cmake_args, "clean")


# +---------------------------------------------------------------------------+


def _cmake_ctest(args: argparse.Namespace, _: typing.List[str]) -> int:
    """
    run ctest
    """
    # we use ctest to run the compile tests so we take a different
    # branch here.
    compile_suite_dir = _build_dir(args) / pathlib.Path("compile")
    report_path = compile_suite_dir / "ctest.xml"
    ctest_run = ["ctest"
                 , "-DCTEST_FULL_OUTPUT"
                 , "--output-junit"
                 , str(report_path)
                 , "-V"
                 , "--test-dir"
                 , str(compile_suite_dir)]
    if not args.dry_run:
        logging.debug("about to run {}".format(str(ctest_run)))
        return subprocess.run(ctest_run, cwd=_build_dir(args)).returncode
    else:
        logging.info("Is dry-run. Would have run ctest: {}".format(str(ctest_run)))
        return 0


# +---------------------------------------------------------------------------+
# | COMMAND-LINE INTERFACE
# +---------------------------------------------------------------------------+


def cli() -> int:
    """
    Main method to execute when this package/script is invoked as a command.
    """
    args = _make_parser().parse_args()

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
    Command-line Arguments to {} for build folder {}:

    {}

    For verify version {}
    *****************************************************************

    """
        ).format(os.path.basename(__file__), str(_build_dir(args)), str(args), _get_version_number(_root_dir(args)))
    )

    # --[CLEAN]----------------------------------------------------------------
    if args.action == "clean" or args.action.prefix == "clean":
        _cmake_clean(args, cmake_args)

    if args.action == "clean":
            return 0

    # --[CONFIGURE]------------------------------------------------------------
    if args.action.only and args.action != "configure":
        logging.debug("{}-only specified. Skipping configure step.".format(str(args.action)))

    else:
        result = _create_build_dir_action(args)
        if result != 0:
            return result

        result = _create_external_dir_action(args)
        if result != 0:
            return result

        result = _cmake_configure(args, cmake_args)
        if result != 0:
            return result

        if args.action == "configure":
            return 0

    # --[BUILD]----------------------------------------------------------------
    if args.action.only and args.action != "build":
        logging.debug("{}-only specified. Skipping build step.".format(str(args.action)))

    else:
        result = _cmake_build(args, cmake_args, args.action.suffix)
        if result != 0:
            return result

        if args.action == "build":
            return 0

    # --[TEST]-----------------------------------------------------------------
    if args.action.only and args.action != "test":
        logging.debug("{}-only specified. Skipping test step.".format(str(args.action)))

    else:

        result = _cmake_test(args, cmake_args, args.action.suffix)
        if result != 0:
            return result

        if args.action == "test":
            return 0

    # --[RELEASE]--------------------------------------------------------------
    if args.action.only and args.action != "release":
        logging.debug("{}-only specified. Skipping release step.".format(str(args.action)))

    else:
        result = _cmake_release(args, cmake_args)
        if result != 0:
            return result

        result = _cmake_install(args, cmake_args)
        if result != 0:
            return result

    return 0


# +---------------------------------------------------------------------------+

if __name__ == "__main__":
    sys.exit(cli())
