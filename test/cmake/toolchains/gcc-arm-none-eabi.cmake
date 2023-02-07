#
# Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# Toolchain definition for gcc-arm-none-eabi embedded cross compiler.
#
# (With some thanks to https://github.com/vpetrigo)
#
cmake_minimum_required(VERSION 3.16.0)

set(CMAKE_SYSTEM_NAME Generic)
SET(CMAKE_SYSTEM_PROCESSOR arm-eabi)

set(TOOLCHAIN_PREFIX arm-none-eabi-)
set(TOOLCHAIN_TRIPLE arm-none-eabi)

set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc CACHE FILEPATH "C compiler")
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++ CACHE FILEPATH "C++ compiler")
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc CACHE FILEPATH "assembler")

execute_process(
        COMMAND which ${CMAKE_C_COMPILER}
        OUTPUT_VARIABLE BINUTILS_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

get_filename_component(ARM_TOOLCHAIN_DIR ${BINUTILS_PATH} DIRECTORY)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_SYSROOT ${ARM_TOOLCHAIN_DIR}/../${TOOLCHAIN_TRIPLE})
set(CMAKE_FIND_ROOT_PATH ${ARM_TOOLCHAIN_DIR}/../${TOOLCHAIN_TRIPLE})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
