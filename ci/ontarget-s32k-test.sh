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
# |
# | NOTE: You must have the environment variable NAIT_UART_DEVICE
# | set to the serial port you are using to monitor hardware
# | tests or supply this as the first argument to this script.
# +----------------------------------------------------------+
mkdir -p build_ci_ontarget_s32k
pushd build_ci_ontarget_s32k

if [ ! -z "$BUILDKITE_BUILD_ID"]; then
    buildkite-agent artifact download "build_ci_ontarget_s32k/*.hex" .
    buildkite-agent artifact download "build_ci_ontarget_s32k/*.jlink" .
    ls -lAh
else
    echo "No BUILDKITE_BUILD_ID. Skipping artifact download."
fi

PORT_VALUE=${1:-$NAIT_UART_DEVICE}

if [ -z "$PORT_VALUE" ]; then
    echo "Either set NAIT_UART_DEVICE in your environment or pass the serial port as the first argument to this script."
    exit 1
fi

nait -vv \
     --port \
    ${PORT_VALUE} \
     --port-speed 115200 \
     \*.jlink

popd
