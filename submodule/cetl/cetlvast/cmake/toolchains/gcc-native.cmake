#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

# Toolchain for using gcc on what-ever-platform-this-is (aka "native").
#
set(CMAKE_C_COMPILER gcc CACHE FILEPATH "C compiler")
set(CMAKE_CXX_COMPILER g++ CACHE FILEPATH "C++ compiler")
set(CMAKE_ASM_COMPILER gcc CACHE FILEPATH "assembler")
set(CMAKE_CXX_STANDARD ${CETLVAST_CPP_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_COVERAGE_PROCESSOR gcov)
