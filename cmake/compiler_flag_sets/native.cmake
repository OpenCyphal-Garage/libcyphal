
#
# Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# C, CXX, LD, and AS flags for building native examples and other typical
# code.
#
include(${CMAKE_SOURCE_DIR}/cmake/compiler_flag_sets/native_common.cmake)

#
# Enable debugging but do not instrument for coverage or enable
# -DDEBUG
#
list(APPEND C_FLAG_SET
                "-Og"
                "-ggdb"
)

list(APPEND CXX_FLAG_SET ${C_FLAG_SET})
list(APPEND ASM_FLAG_SET ${C_FLAG_SET})
