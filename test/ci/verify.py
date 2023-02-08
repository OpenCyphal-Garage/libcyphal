#!/usr/bin/env python3
#
# Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
# Copyright (C) 2018-2023  OpenCyphal Development Team  <opencyphal.org>
# This software is distributed under the terms of the MIT License.
#
"""
    Command-line helper for running verification builds.
"""

import abc
import argparse
import logging
import os
import pathlib
import shutil
import subprocess
import sys
import textwrap
import typing

# +---------------------------------------------------------------------------+
# | ACTIONS
# +---------------------------------------------------------------------------+


class CMakeAction(abc.ABC):
    """Base class for all verify actions that invoke cmake."""

    # +-----------------------------------------------------------------------+
    # | VISIT ARGPARSE
    # +-----------------------------------------------------------------------+
    @classmethod
    def visit_add_parser(self, sub_parsers: argparse._SubParsersAction) -> typing.Optional[argparse.ArgumentParser]:
        return None

    @classmethod
    def visit_setargs(self, parser: argparse.ArgumentParser) -> None:
        build_args = parser.add_argument_group(
            title="build options",
            description=textwrap.dedent(
                """
            Arguments that can be used in parallel builds. Each of these
            will change the name of the build directory created for the
            build.
        """[
                    1:
                ]
            ),
        )

        build_args.add_argument(
            "--project-root",
            type=pathlib.Path,
            help=textwrap.dedent(
                """
            (cmake LIBCYPHAL_PROJECT_ROOT) Set the root directory used as the
            libcyphal repository. If omitted the root is implied to be the
            current working directory.

        """[
                    1:
                ]
            ),
        )

        build_args.add_argument(
            "--build-type",
            type=str,
            choices=["Release", "Debug"],
            default="Debug",
            help=textwrap.dedent(
                """
            (cmake CMAKE_BUILD_TYPE) The build flavor to configure, build, and test.

        """[
                    1:
                ]
            ),
        )

        build_args.add_argument(
            "--target",
            type=str,
            choices=["native", "s32k3"],
            default="native",
            help=textwrap.dedent(
                """
            (cmake LIBCYPHAL_FLAG_SET) The target platform.

        """[
                    1:
                ]
            ),
        )

        build_args.add_argument(
            "--toolchain-family",
            type=str,
            choices=["clang", "gcc"],
            default="clang",
            help=textwrap.dedent(
                """
            The toolchain family to find and use.

        """[
                    1:
                ]
            ),
        )

        build_args.add_argument(
            "-x",
            "--no-coverage",
            action="store_true",
            help="Disables generation of test coverage data. This is enabled by default.",
        )

        action_args = parser.add_argument_group(
            title="behavioral options",
            description=textwrap.dedent(
                """
            Arguments that change the actions taken by a command.
        """[
                    1:
                ]
            ),
        )

        action_args.add_argument("-v", "--verbose", action="count", default=0, help="Set output verbosity.")

        action_args.add_argument(
            "-f",
            "--force",
            action="store_true",
            help=textwrap.dedent(
                """
            Force an operation. This flag is interpreted by the command given.

        """[
                    1:
                ]
            ),
        )

        action_args.add_argument(
            "--dry-run",
            action="store_true",
            help=textwrap.dedent(
                """
            Don't actually do anything. Just log what this script would have done.
            Combine with --verbose to ensure you actually see the script's log output.
        """[
                    1:
                ]
            ),
        )

        other_options = parser.add_argument_group(
            title="extra options",
            description=textwrap.dedent(
                """
            Additional arguments for modifying how commands run but which are
            used less frequently.
        """[
                    1:
                ]
            ),
        )

        other_options.add_argument(
            "--suffix",
            type=str,
            default="ci",
            help=textwrap.dedent(
                """
            The top level directory managed by this script starts with
            "build_{suffix}" where the default suffix is "ci". This argument
            changes this suffix.

            Note that the "build_{suffix}" prefix is used by the clean action
            to identify folders to delete.

        """[
                    1:
                ]
            ),
        )

        other_options.add_argument(
            "-j",
            "--jobs",
            type=int,
            help="The number of concurrent build jobs to request. "
            "Defaults to the number of logical CPUs on the local machine.",
        )

        other_options.add_argument("--verification-dir", default="test", help="Path to the verification directory.")

        other_options.add_argument(
            "--use-default-generator",
            action="store_true",
            help=textwrap.dedent(
                """
            We use Ninja by default. Set this flag to omit the explicit generator override
            and use whatever the default is for cmake (i.e. normally make)
        """[
                    1:
                ]
            ),
        )

    # +-----------------------------------------------------------------------+
    # | ACTION INTERFACE
    # +-----------------------------------------------------------------------+

    @abc.abstractmethod
    def run(self, args: argparse.Namespace) -> int:
        pass

    # +-----------------------------------------------------------------------+
    # | PROTECTED METHODS
    # +-----------------------------------------------------------------------+
    def _create_cmake_args(self, args: argparse.Namespace) -> typing.List[str]:
        return ["cmake"]

    def _get_cmake_logging_level(self, args: argparse.Namespace) -> str:
        cmake_logging_level = "NOTICE"

        if args.verbose == 1:
            cmake_logging_level = "STATUS"
        elif args.verbose == 2:
            cmake_logging_level = "VERBOSE"
        elif args.verbose == 3:
            cmake_logging_level = "DEBUG"
        elif args.verbose > 3:
            cmake_logging_level = "TRACE"

        return cmake_logging_level

    def _create_build_dir_prefix(self, args: argparse.Namespace) -> str:
        return "build_{}".format(args.suffix)

    def _create_build_dir_name(self, args: argparse.Namespace) -> str:
        name = self._create_build_dir_prefix(args)

        name += "_{}".format(args.toolchain_family)

        name += "_{}".format(args.target)

        name += "_{}".format(args.build_type)

        return name

    def _create_verification_dir_path(self, args: argparse.Namespace) -> pathlib.Path:
        if args.project_root is not None and args.project_root != "":
            project_root = pathlib.Path(args.project_root)
        else:
            project_root = pathlib.Path.cwd()

        return project_root / pathlib.Path(args.verification_dir)

    def _create_cmake_path(self, args: argparse.Namespace) -> pathlib.Path:
        return self._create_verification_dir_path(args) / pathlib.Path(self._create_build_dir_name(args))

    def _cmake_run(
        self,
        cmake_args: typing.List[str],
        cmake_dir: pathlib.Path,
        verbose: int,
        dry_run: bool,
        env: typing.Optional[typing.Dict] = None,
    ) -> int:
        """
        Simple wrapper around cmake execution logic
        """
        logging.debug(
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

    def _delete_dir(self, args: argparse.Namespace, dir_to_delete: pathlib.Path) -> bool:
        """Common logic for delete director actions.

        Args:
            args (argparse.Namespace): Consumes force and dry-run arguments from this namespace.
            dir_to_delete (pathlib.Path): The directory to, potentially, delete.

        Returns:
            bool: True if the directory was or would have been removed. False if the directory was not
                  or would not have been removed.
        """
        did_or_would_removed = False
        if dir_to_delete.exists():
            if not args.force:
                response = input("Are you sure you want to delete {}? [y/N]:".format(dir_to_delete))
                if (len(response) == 1 and response.lower() == "y") or (
                    len(response) == 3 and response.lower() == "yes"
                ):
                    did_or_would_removed = True
            else:
                did_or_would_removed = True

            if did_or_would_removed:
                if not args.dry_run:
                    logging.info("Removing directory {}".format(dir_to_delete))
                    shutil.rmtree(dir_to_delete)
                else:
                    logging.info("Is dry-run. Would have removed directory {}".format(dir_to_delete))

        return did_or_would_removed

    def _handle_build_dir(self, args: argparse.Namespace, cmake_dir: pathlib.Path) -> None:
        """
        Handle all the logic, user input, logging, and file-system operations needed to
        manage the cmake build directory ahead of invoking cmake.
        """
        if args.remove_first and cmake_dir.exists():
            if not self._delete_dir(args, cmake_dir):
                raise RuntimeError(
                    """
                    Build directory {} already exists, -rm or --remove-first was specified,
                    and permission was not granted to delete it. We cannot continue. Either
                    allow re-use of this build directory or allow deletion. (use -f flag to
                    skip user prompts).""".lstrip().format(
                        cmake_dir
                    )
                )

        if not cmake_dir.exists():
            if not args.dry_run:
                logging.info("Creating build directory at {}".format(cmake_dir))
                cmake_dir.mkdir()
            else:
                logging.info("Dry run: Would have created build directory at {}".format(cmake_dir))
        else:
            logging.info("Using existing build directory at {}".format(cmake_dir))


# +---------------------------------------------------------------------------+


class CMakeConfigure(CMakeAction):
    # +-----------------------------------------------------------------------+
    # | VISIT ARGPARSE
    # +-----------------------------------------------------------------------+
    @classmethod
    def visit_add_parser(self, sub_parsers: argparse._SubParsersAction) -> typing.Optional[argparse.ArgumentParser]:
        return sub_parsers.add_parser("configure", help="CMake configuration commands")

    @classmethod
    def visit_setargs(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument(
            "-rm",
            "--remove-first",
            action="store_true",
            help=textwrap.dedent(
                """
            If specified, any existing build directory will be deleted first. Use
            -f to skip the user prompt.

            Note: This only applies to the configure step. If you do a build-only this
            argument has no effect.
        """[
                    1:
                ]
            ),
        )

        parser.add_argument(
            "--omit-style-check-from-all",
            action="store_true",
            help=textwrap.dedent(
                """
            (cmake LIBCYPHAL_STYLE_CHECK)
            If specified then the style check rule will not be added to the "all" list
            of build rules.
        """[
                    1:
                ]
            ),
        )

        # TODO:
        # LIBCYPHAL_ENABLE_EXCEPTIONS
        # LIBCYPHAL_INTROSPECTION_ENABLE_ASSERT
        # LIBCYPHAL_INTROSPECTION_TRACE_ENABLE
        # language standard

    # +-----------------------------------------------------------------------+
    # | ACTION INTERFACE
    # +-----------------------------------------------------------------------+
    def run(self, args: argparse.Namespace) -> int:
        """
        Format and execute cmake configure command. This also include the cmake build directory (re)creation
        logic.
        """
        cmake_dir = self._create_cmake_path(args)

        self._handle_build_dir(args, cmake_dir)

        cmake_configure_args = self._create_cmake_args(args)

        cmake_configure_args.append("--log-level={}".format(self._get_cmake_logging_level(args)))
        cmake_configure_args.append("-DCMAKE_BUILD_TYPE={}".format(args.build_type))

        if args.verbose > 1:
            cmake_configure_args.append("-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON")

        flag_set_dir = pathlib.Path("cmake") / pathlib.Path("compiler_flag_sets")
        if args.target == "s32k3":
            flag_set_filename = pathlib.Path("arm-none-eabi-m7").with_suffix(".cmake")
        else:
            flag_set_filename = pathlib.Path("native").with_suffix(".cmake")

        flagset_file = flag_set_dir / flag_set_filename

        if hasattr(args, "project_root") and args.project_root is not None:
            cmake_configure_args.append("-DLIBCYPHAL_PROJECT_ROOT={}".format(args.project_root))

        cmake_configure_args.append("-DLIBCYPHAL_FLAG_SET={}".format(str(flagset_file)))

        if args.no_coverage:
            cmake_configure_args.append("-DLIBCYPHAL_ENABLE_COVERAGE:BOOL=OFF")
        else:
            cmake_configure_args.append("-DLIBCYPHAL_ENABLE_COVERAGE:BOOL=ON")

        if args.omit_style_check_from_all:
            cmake_configure_args.append("-DLIBCYPHAL_STYLE_CHECK:BOOL=OFF")

        toolchain_dir = pathlib.Path("cmake") / pathlib.Path("toolchains")
        if args.target == "s32k3":
            compiler_triple = "arm-none-eabi"
        else:
            compiler_triple = "native"

        toolchain_file = toolchain_dir / pathlib.Path(
            "{}-{}".format(args.toolchain_family, compiler_triple)
        ).with_suffix(".cmake")

        cmake_configure_args.append("-DCMAKE_TOOLCHAIN_FILE={}".format(str(toolchain_file)))

        if not args.use_default_generator:
            cmake_configure_args.append("-DCMAKE_GENERATOR=Ninja")

        cmake_configure_args.append("..")

        return self._cmake_run(cmake_configure_args, cmake_dir, args.verbose, args.dry_run)


# +---------------------------------------------------------------------------+


class CMakeBuild(CMakeAction):
    # +-----------------------------------------------------------------------+
    # | VISIT ARGPARSE
    # +-----------------------------------------------------------------------+
    @classmethod
    def visit_add_parser(self, sub_parsers: argparse._SubParsersAction) -> typing.Optional[argparse.ArgumentParser]:
        return sub_parsers.add_parser("build", help="CMake build commands")

    @classmethod
    def visit_setargs(self, parser: argparse.ArgumentParser) -> None:
        pass

    # +-----------------------------------------------------------------------+
    # | ACTION INTERFACE
    # +-----------------------------------------------------------------------+
    def run(self, args: argparse.Namespace) -> int:
        """
        Format and execute cmake build command. This method assumes that the cmake_dir is already properly
        configured.
        """
        cmake_dir = self._create_cmake_path(args)

        cmake_build_args = self._create_cmake_args(args)

        cmake_build_args += ["--build", ".", "--target", "all"]

        if args.jobs is not None and args.jobs > 0:
            cmake_build_args += ["--", "-j{}".format(args.jobs)]

        return self._cmake_run(cmake_build_args, cmake_dir, args.verbose, args.dry_run)


# +---------------------------------------------------------------------------+


class CMakeTest(CMakeAction):
    # +-----------------------------------------------------------------------+
    # | VISIT ARGPARSE
    # +-----------------------------------------------------------------------+
    @classmethod
    def visit_add_parser(self, sub_parsers: argparse._SubParsersAction) -> typing.Optional[argparse.ArgumentParser]:
        return sub_parsers.add_parser("test", help="CMake test commands")

    @classmethod
    def visit_setargs(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("test-target")

    # +-----------------------------------------------------------------------+
    # | ACTION INTERFACE
    # +-----------------------------------------------------------------------+
    def run(self, args: argparse.Namespace) -> int:
        """
        Format and execute cmake test command. This method assumes that the cmake_dir is already properly
        configured.
        """

        cmake_dir = self._create_cmake_path(args)

        cmake_test_args = self._create_cmake_args(args)

        cmake_test_args += ["--build", ".", "--target"]

        cmake_test_args.append(getattr(args, "test-target"))

        return self._cmake_run(cmake_test_args, cmake_dir, args.verbose, args.dry_run)


# +---------------------------------------------------------------------------+


class CMakeCompileTest(CMakeAction):
    # +-----------------------------------------------------------------------+
    # | VISIT ARGPARSE
    # +-----------------------------------------------------------------------+
    @classmethod
    def visit_add_parser(self, sub_parsers: argparse._SubParsersAction) -> typing.Optional[argparse.ArgumentParser]:
        return sub_parsers.add_parser("compile-test", help="Run compile-time tests.")

    @classmethod
    def visit_setargs(self, parser: argparse.ArgumentParser) -> None:
        pass

    # +-----------------------------------------------------------------------+
    # | ACTION INTERFACE
    # +-----------------------------------------------------------------------+
    def run(self, args: argparse.Namespace) -> int:
        """
        Format and execute cmake test command. This method assumes that the cmake_dir is already properly
        configured.
        """

        cmake_dir = self._create_cmake_path(args)

        ctest_test_args = ["ctest", "-VV"]

        return self._cmake_run(ctest_test_args, cmake_dir, args.verbose, args.dry_run)


# +---------------------------------------------------------------------------+


class CMakeClean(CMakeAction):
    # +-----------------------------------------------------------------------+
    # | VISIT ARGPARSE
    # +-----------------------------------------------------------------------+
    @classmethod
    def visit_add_parser(self, sub_parsers: argparse._SubParsersAction) -> typing.Optional[argparse.ArgumentParser]:
        return sub_parsers.add_parser("clean", help="CMake clean commands")

    @classmethod
    def visit_setargs(self, parser: argparse.ArgumentParser) -> None:
        pass

    # +-----------------------------------------------------------------------+
    # | ACTION INTERFACE
    # +-----------------------------------------------------------------------+
    def run(self, args: argparse.Namespace) -> int:
        """
        Nuke all cmake stuff.
        """

        verification_dir = self._create_verification_dir_path(args)
        managed_dirs = [
            dir
            for dir in verification_dir.glob("**/{}*".format(self._create_build_dir_prefix(args), recursive=True))
            if dir.is_dir()
        ]

        if len(managed_dirs) == 0:
            logging.info("Nothing to clean.")
        else:
            for file in managed_dirs:
                self._delete_dir(args, file)

        return 0


# +---------------------------------------------------------------------------+
# | SETUP ARGUMENTS
# +---------------------------------------------------------------------------+


def _make_parser() -> argparse.ArgumentParser:
    epilog = textwrap.dedent(
        """

        **Example Usage**::

            ./test/ci/verify.py configure

    """
    )

    parser = argparse.ArgumentParser(
        description="CMake command-line helper for running verification builds.",
        epilog=epilog,
        formatter_class=argparse.RawTextHelpFormatter,
    )

    commands = parser.add_subparsers(title="Commands", dest="command")

    for action in [CMakeAction, CMakeConfigure, CMakeBuild, CMakeTest, CMakeCompileTest, CMakeClean]:
        command_parser = action.visit_add_parser(commands)
        if command_parser is None:
            action.visit_setargs(parser)
        else:
            action.visit_setargs(command_parser)
            command_parser.set_defaults(_runner=action)

    return parser


# +---------------------------------------------------------------------------+
# | CLI
# +---------------------------------------------------------------------------+


def main() -> int:
    """
    Main method to execute when this package/script is invoked as a command.
    """
    parser = _make_parser()
    args = parser.parse_args()

    logging_level = logging.WARN

    if args.verbose == 1:
        logging_level = logging.INFO
    elif args.verbose > 1:
        logging_level = logging.DEBUG

    logging.basicConfig(format="%(levelname)s: %(message)s", level=logging_level)

    logging.debug(
        textwrap.dedent(
            """

    *****************************************************************
    Commandline Arguments to {}:

    {}

    *****************************************************************

    """
        ).format(os.path.basename(__file__), str(args))
    )

    if not hasattr(args, "_runner"):
        parser.print_help()
        return 0
    else:
        runner: CMakeAction = args._runner()
        return runner.run(args)


if __name__ == "__main__":
    sys.exit(main())
