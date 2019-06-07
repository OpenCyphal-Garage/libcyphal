#!/usr/bin/env python3
#
# Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
# Copyright (C) 2019  UAVCAN Development Team  <uavcan.org>
# This software is distributed under the terms of the MIT License.
#

import sys
import setuptools

if int(setuptools.__version__.split('.')[0]) < 30:
    print('A newer version of setuptools is required. The current version does not support declarative config.',
          file=sys.stderr)
    sys.exit(1)

setuptools.setup(version='1.0.0')
