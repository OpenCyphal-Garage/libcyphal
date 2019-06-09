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

mkdir -p build_native_gcc
pushd build_native_gcc

cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/gcc-native.cmake \
      -DLIBUAVCAN_FLAG_SET=../cmake/compiler_flag_sets/native_unittest.cmake \
      -DLIBUAVCAN_INTROSPECTION_ENABLE_ASSERT=1 \
      ..

make -j4

# We use ctest to run our compile tests.
ctest -VV

# This builds, runs, and reports on our native unit tests.
make cov_all

make docs

popd
