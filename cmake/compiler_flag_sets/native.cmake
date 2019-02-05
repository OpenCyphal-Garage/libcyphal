#
# Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# C, CXX, LD, and AS flags for native targets.
#

#
# Debug flags for C and C++
#
set(C_FLAG_SET
                "-fprofile-arcs"
                "-ftest-coverage"
                "-Og"
                "-DDEBUG"
                "-ggdb"
)

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
                "-Wabi"
                "-Wunused-parameter"
                "-Wunused-variable"
                "-Wunused-value"
                "-Wcast-align"
                "-Wmissing-declarations"
                "-Wno-missing-field-initializers"
                "-Wdouble-promotion"
                "-Wswitch-enum"
                "-Wtype-limits"
                "-Wno-error=array-bounds"
)

set(CXX_FLAG_SET ${C_FLAG_SET})
set(ASM_FLAG_SET ${C_FLAG_SET})

#
# General C-only flags
#
list(APPEND C_FLAG_SET
                "-std=gnu99"
)

#
# General C++ only flags
#
list(APPEND CXX_FLAG_SET
                "-std=c++11"
                "-fno-rtti"
                "-fno-exceptions"
                "-fno-threadsafe-statics"
)

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

set(EXE_LINKER_FLAG_SET )
set(DEFINITIONS_SET )
