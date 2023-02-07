
#
# Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# C, CXX, LD, and AS flags for building native unit tests. These flags also include
# instrumentation for code coverage.
#
include(${CMAKE_SOURCE_DIR}/cmake/compiler_flag_sets/_native_common.cmake)


if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
    message(STATUS "Release build. Setting optimization flags.")
    list(APPEND C_FLAG_SET
                "-O1"
    )
else()
    message(STATUS "Not a Release build. Setting debug flags.")
    list(APPEND C_FLAG_SET
                "-O0"
                "-DDEBUG"
                "-ggdb"
    )
endif()

list(APPEND CXX_FLAG_SET ${C_FLAG_SET})
list(APPEND ASM_FLAG_SET ${C_FLAG_SET})

list(APPEND CXX_FLAG_SET
                "-fno-exceptions"
                "-fno-rtti"
)
