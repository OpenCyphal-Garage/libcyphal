#
# Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# Toolchain for using clang on what-ever-platform-this-is (aka "native").
# CMake and/or Ubuntu defaults to gcc.
#
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_COMPILER clang)
