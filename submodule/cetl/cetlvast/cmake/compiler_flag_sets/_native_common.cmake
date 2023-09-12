#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

# C, CXX, LD, and AS flags for native targets.
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

if (CETLVAST_ENABLE_COVERAGE)
message(STATUS "Coverage is enabled. Instrumenting the code.")
list(APPEND C_FLAG_SET
                "--coverage"
                "$<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang>:-fprofile-instr-generate>"
                "$<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang>:-fcoverage-mapping>"
)

list(APPEND EXE_LINKER_FLAG_SET "--coverage")

endif()

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
