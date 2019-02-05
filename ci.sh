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
# | This script is the common set of commands run as part of
# | a continuous integration build. It should be run inside
# | of the uavcan/libuavcan container.
# |
# | Of course, libuavcan is a header-only distribution so
# | CI is used to verify and test rather than package and
# | deploy (i.e. There's really no 'I' going on).
# +----------------------------------------------------------+

mkdir -p build_ci_native
pushd build_ci_native
# We build native tests using clang since we use gcc for
# cross-compiling. This gives us coverage by two different
# compilers.
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/clang-native.cmake ..
make -j16
ctest -VV

popd

# Do this all again but for our "on target" build and test.
mkdir -p build_ci_ontarget
pushd build_ci_ontarget

# For now we only conpile for arm. In the future we'll actually run the compiled
# tests on-target.
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/gcc-arm-none-eabi.cmake \
      -DGTEST_USE_LOCAL_BUILD=ON \
      -DLIBUAVCAN_FLAG_SET=../cmake/compiler_flag_sets/cortex-m7-fpv5-d16-nano.cmake \
      -DLIBUAVCAN_TESTBUILD=../test/ontarget/unit_tests.cmake \
      ..

make -j16
