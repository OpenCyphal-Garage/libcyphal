#
# Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# C, CXX, LD, and AS flags for native targets.
#

set(MCU_ARCH cortex-m7)
set(MCU_FLOAT_ABI hard)

set(C_FLAG_SET  "-mcpu=${MCU_ARCH}"
                "-mthumb"
                "-mno-thumb-interwork"
                "-mfpu=fpv5-d16"
                "-mfloat-abi=${MCU_FLOAT_ABI}"
                "-ffunction-sections"
                "-fdata-sections"
                "-fno-strict-aliasing"
)

#
# Debug flags for C and C++
#
list(APPEND C_FLAG_SET
                "-Og"
                "-DDEBUG"
                "-ggdb"
)

#
# Warnings and other diagnostics for C and C++
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
                "-Wno-missing-field-initializers"
                "-Wdouble-promotion"
                "-Wtype-limits"
                "-Wno-error=array-bounds"
)

set(CXX_FLAG_SET ${C_FLAG_SET})
set(ASM_FLAG_SET ${C_FLAG_SET})

list(APPEND C_FLAG_SET
                "-std=gnu99"
)

#
# Note that you must use gnu++11 to build googletest using gcc-arm-none-eabi. Not
# exactly sure why but this seems a reasonable concession given that we only support
# this one cross compiler for our on-target tests. Nothing about this should
# limit use of libuavcan itself for applications using clang, IAR, ARM, or 
# any other c++11 compliant compiler. This is _only_ a googletest limitation.
#
list(APPEND CXX_FLAG_SET
                "-std=gnu++11"
                "-fno-rtti"
                "-fno-exceptions"
                "-fno-threadsafe-statics"
)

list(APPEND CXX_FLAG_SET
                "-Wcast-align"
                "-Wsign-promo"
                "-Wold-style-cast"
                "-Wzero-as-null-pointer-constant"
                "-Wnon-virtual-dtor"
                "-Woverloaded-virtual"
)

set(EXE_LINKER_FLAG_SET
                "--specs=nano.specs"
                "--specs=nosys.specs"
                "-Wl,-gc-sections"
                "-Wl,--print-memory-usage"
                "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map"
                "-Wl,--start-group"
                "-L${CMAKE_CURRENT_SOURCE_DIR}/test/ontarget/CMSIS/Lib/GCC"
                "-larm_cortexM7lfdp_math"
                "-lm"
                "-Wl,--end-group"
)

set(DEFINITIONS_SET 
                "-DARM_MATH_CM7=true"
                "-D__FPU_PRESENT=1"
)
