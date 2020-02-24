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
# |   SONARQUBE_TOKEN = A token that will enable sonarqube uploads.
# |   BUILDKITE = Set to "true" when building within a Buildkite pipeline.
# |
# +----------------------------------------------------------+

if [ -z "${BUILDKITE_BUILD_NUMBER+xxx}" ]; then
BUILDKITE_BUILD_NUMBER="xxx"
fi

# +----------------------------------------------------------+

if [ -z "${BUILDKITE+xxx}" ]; then
    echo "Not BUILDKITE. Skipping artifact download."
    mkdir -p test/build_native_gcc
else
    buildkite-agent artifact download "test/build_native_gcc/tests/coverage.info" .
    buildkite-agent artifact download "test/build_native_gcc/build-wrapper-dump.json" .
fi

echo "+--------------------------------------------------------------+"
echo "| The contents of test/build_native_gcc"
echo "+--------------------------------------------------------------+"

ls -lah test/build_native_gcc

echo "+--------------------------------------------------------------+"

if [ -z "${SONARQUBE_TOKEN+xxx}" ]; then
    echo "No SONARQUBE_TOKEN defined. Skipping upload."
else
    sonar-scanner \
        -Dsonar.login=${SONARQUBE_TOKEN} \
        -Dsonar.buildString=${BUILDKITE_BUILD_NUMBER} \
        -Dsonar.working.directory=test/build_native_gcc/.scannerwork \
        -Dsonar.cfamily.build-wrapper-output=test/build_native_gcc
fi
