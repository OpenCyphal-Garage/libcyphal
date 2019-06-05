#!/usr/bin/env bash

# +----------------------------------------------------------+
# | BASH : Modifying Shell Behaviour
# |    (https://www.gnu.org/software/bash/manual)
# +----------------------------------------------------------+
# Treat unset variables and parameters other than the special
# parameters ‘@’ or ‘*’ as an error when performing parameter
# expansion. An error message will be written to the standard
# error, and a non-interactive shell will exit.
set -o nounset

# Exit immediately if a pipeline returns a non-zero status.
set -o errexit

# If set, the return value of a pipeline is the value of the
# last (rightmost) command to exit with a non-zero status, or
# zero if all commands in the pipeline exit successfully.
set -o pipefail

# +----------------------------------------------------------+
# | This script is one of the common set of commands run as
# | part of a continuous integration build pipeline.
# | These scrips are named using the following scheme:
# |
# |   [build_type]-[(optional)build_type qualifier]-[build|test|report|upload].sh
# |
# | Of course, libuavcan is a header-only distribution so
# | CI is used to verify and test rather than package and
# | deploy (i.e. There's really no 'I' going on).
# +----------------------------------------------------------+

mkdir -p build_ci_ontarget_s32k
pushd build_ci_ontarget_s32k

# For now we only conpile for arm. In the future we'll actually run the compiled
# tests on-target.
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/gcc-arm-none-eabi.cmake \
      -DGTEST_USE_LOCAL_BUILD=ON \
      -DLIBUAVCAN_FLAG_SET=../cmake/compiler_flag_sets/cortex-m4-fpv4-sp-d16-nosys.cmake \
      -DLIBUAVCAN_TESTBUILD=../test/ontarget/S32K148EVB/unit_tests.cmake \
      -DLIBUAVCAN_SKIP_DOCS=ON \
      -DLIBUAVCAN_EXT_FOLDER=build_ci_ext \
      ..

make -j4

# We use ctest to run our compile tests.
ctest -VV

popd
