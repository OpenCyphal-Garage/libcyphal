#!/usr/bin/env bash

# +----------------------------------------------------------+
# | BASH : Modifying Shell Behaviour
# |    (https://www.gnu.org/software/bash/manual)
# +----------------------------------------------------------+

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
mkdir -p build_ci_native_gcc
pushd build_ci_native_gcc

if [ -z "$BUILDKITE_PULL_REQUEST" ]; then
    buildkite-agent artifact download "build_ci_native_gcc/docs/html.gz" .
    tar -xvf docs/html.gz
    gh-pages --dotfiles --message "Doc upload for build ${BUILDKITE_BUILD_NUMBER}" --user "uavcan1.0 <uavcan1.0@uavcan.org>" --dist docs/html
else
    echo "Skipping doc upload for pull-requests."
fi

buildkite-agent artifact download "build_ci_native_gcc/tests/coverage.info" .

# Our custom lcov tracefile and coveralls.io upload script.
# This is only available in our docker container.
info_to_coveralls --root ../ tests/coverage.info

popd
