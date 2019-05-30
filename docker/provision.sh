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
export DEBIAN_FRONTEND=noninteractive

apt-get update
apt-get -y install software-properties-common
add-apt-repository ppa:team-gcc-arm-embedded/ppa -y
apt-get update
apt-get -y install apt-utils
apt-get -y install python3
apt-get -y install python-pip
apt-get -y install python3-venv
apt-get -y install cmake
apt-get -y install git
apt-get -y install gcc-arm-embedded
apt-get -y install clang
apt-get -y install clang-format
apt-get -y install doxygen
apt-get -y install lcov
apt-get -y install valgrind
apt-get -y install clang-tidy
apt-get -y install npm

pip install virtualenv

npm install -g gh-pages
