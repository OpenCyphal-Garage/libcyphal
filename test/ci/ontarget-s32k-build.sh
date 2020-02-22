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
# | Standard parameters:
# |
# |   CMAKE_BUILD_TYPE = either 'Release' or 'Debug'. The
# |         default is Debug
# |   MAKEFILE_JOBS_COMPILATION = number of concurrent jobs
# |         to use when compiling using Unix Makefiles. The
# |         default is 1
# +----------------------------------------------------------+

if [ -z "${CMAKE_BUILD_TYPE+xxx}" ]; then
CMAKE_BUILD_TYPE=Debug
fi

if [ -z "${MAKEFILE_JOBS_COMPILATION+xxx}" ]; then
MAKEFILE_JOBS_COMPILATION=1
fi

# +----------------------------------------------------------+

mkdir -p test/build_ontarget_s32k
pushd test/build_ontarget_s32k

cmake --no-warn-unused-cli \
      -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
      -DCMAKE_TOOLCHAIN_FILE:FILEPATH=../cmake/toolchains/gcc-arm-none-eabi.cmake \
      -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE} \
      -DGTEST_USE_LOCAL_BUILD=ON \
      -DLIBUAVCAN_FLAG_SET=../cmake/compiler_flag_sets/cortex-m4-fpv4-sp-d16-nosys.cmake \
      -DLIBUAVCAN_TESTBUILD=../ontarget/S32K148EVB/tests.cmake \
      -DLIBUAVCAN_SKIP_DOCS=ON \
      -G "Unix Makefiles" \
      ..

cmake --build .\
      --config ${CMAKE_BUILD_TYPE} \
      --target all \
      -- -j${MAKEFILE_JOBS_COMPILATION}

# We use ctest to run our compile tests.
ctest -VV

popd
