
#
# Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# C, CXX, LD, and AS flags for building native unit tests. These flags also include
# instrumentation for code coverage.
#
include(${CMAKE_SOURCE_DIR}/cmake/compiler_flag_sets/native_common.cmake)

#
# Debug and coverage flags for C and C++
#
list(APPEND C_FLAG_SET
                "-fprofile-arcs"
                "-ftest-coverage"
                "--coverage"
                "-O0"
                "-DDEBUG"
                "-ggdb"
)

list(APPEND CXX_FLAG_SET ${C_FLAG_SET})
list(APPEND ASM_FLAG_SET ${C_FLAG_SET})

list(APPEND CXX_FLAG_SET
                "-fno-exceptions"
)
