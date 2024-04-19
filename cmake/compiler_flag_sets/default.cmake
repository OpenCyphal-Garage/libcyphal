#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

# C, CXX, LD, and AS flags for building native unit tests. These flags also include
# instrumentation for code coverage.
#

set(C_FLAG_SET )
set(EXE_LINKER_FLAG_SET )
set(DEFINITIONS_SET )

#
# Diagnostics for C and C++
#
list(APPEND C_FLAG_SET
                "-pedantic"
                "-Wall"
                "-Wextra"
                "-Werror"
                "-Wfloat-equal"
                "-Wconversion"
                "-Wunused-parameter"
                "-Wunused-variable"
                "-Wunused-value"
                "-Wcast-align"
                "-Wmissing-declarations"
                "-Wmissing-field-initializers"
                "-Wdouble-promotion"
                "-Wswitch-enum"
                "-Wtype-limits"
                "-Wno-error=array-bounds"
)

set(CXX_FLAG_SET ${C_FLAG_SET})
set(ASM_FLAG_SET ${C_FLAG_SET})

#
# C++ only diagnostics
#
list(APPEND CXX_FLAG_SET
                "-Wsign-conversion"
                "-Wsign-promo"
                "-Wold-style-cast"
                "-Wzero-as-null-pointer-constant"
                "-Wnon-virtual-dtor"
                "-Woverloaded-virtual"
)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    message(DEBUG "Release build. Setting optimization flags.")
    list(APPEND C_FLAG_SET
                "-O1"
                "-fno-delete-null-pointer-checks"
    )
else()

    message(DEBUG "Not a Release build. Setting debug flags.")
    list(APPEND C_FLAG_SET
                "-O0"
                "-DDEBUG"
                "-ggdb"
    )

endif()

if (CETLVAST_DISABLE_CPP_EXCEPTIONS)
    message(DEBUG "CETLVAST_DISABLE_CPP_EXCEPTIONS is true. Adding -fno-exceptions to compiler flags.")
    list(APPEND CXX_FLAG_SET
                "-fno-exceptions")
endif()

list(APPEND CXX_FLAG_SET ${C_FLAG_SET})
list(APPEND ASM_FLAG_SET ${C_FLAG_SET})

add_compile_options("$<$<COMPILE_LANGUAGE:C>:${C_FLAG_SET}>")
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAG_SET}>")
add_compile_options("$<$<COMPILE_LANGUAGE:ASM>:${ASM_FLAG_SET}>")
add_link_options(${EXE_LINKER_FLAG_SET})
add_definitions(${DEFINITIONS_SET})
