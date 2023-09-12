#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

# C, CXX, LD, and AS flags for building native unit tests. These flags also include
# instrumentation for code coverage.
#
include(${CMAKE_SOURCE_DIR}/cmake/compiler_flag_sets/_native_common.cmake)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
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

    if (CETL_ENABLE_DEBUG_ASSERT)
        message(STATUS "CETL_ENABLE_DEBUG_ASSERT will be 1 enabling debug assert() in cetl code.")
        list(APPEND C_FLAG_SET
            "-DCETL_ENABLE_DEBUG_ASSERT=1")
    endif()
endif()

list(APPEND CXX_FLAG_SET ${C_FLAG_SET})
list(APPEND ASM_FLAG_SET ${C_FLAG_SET})

add_compile_options("$<$<COMPILE_LANGUAGE:C>:${C_FLAG_SET}>")
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAG_SET}>")
add_compile_options("$<$<COMPILE_LANGUAGE:ASM>:${ASM_FLAG_SET}>")
add_link_options(${EXE_LINKER_FLAG_SET})
add_definitions(${DEFINITIONS_SET})
