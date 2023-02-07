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
# | Of course, libcyphal is a header-only distribution so
# | CI is used to verify and test rather than package and
# | deploy (i.e. There's really no 'I' going on).
# |
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

mkdir -p test/build_linux
pushd test/build_linux
# We ensure we can build using clang but we rely on GCC for testing
# since clang's coverage metrics have been broken for the last
# several years.
cmake -DLIBCYPHAL_TESTBUILD=../linux/tests.cmake \
      ..

cmake --build .\
      --config ${CMAKE_BUILD_TYPE} \
      --target all \
      -- -j${MAKEFILE_JOBS_COMPILATION}

popd
