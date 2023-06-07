#!/usr/bin/env python3
#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#
"""
    Command-line helper for running verification builds.
"""

import sys

sys.path.append("external/cetl/build-tools/bin")

from cetlvast import cli as cetlvast_cmake_cli

if __name__ == "__main__":
    sys.exit(cetlvast_cmake_cli())
