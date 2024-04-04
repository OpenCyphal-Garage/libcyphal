# Copyright Amazon.com Inc., or its affiliates. All Rights Reserved.

if (NOT TARGET cyphal)

include(ProjectLibrary)
find_package(libcanard REQUIRED)
find_package(libudpard REQUIRED)
find_package(cetl REQUIRED)

# +---------------------------------------------------------------------------+
# | (lib)cyphal
# +---------------------------------------------------------------------------+
cmake_path(APPEND LIBCYPHAL_ROOT "include" OUTPUT_VARIABLE LIBCYPHAL_INCLUDE)

add_project_library(
    NAME cyphal
    HEADER_PATH
        ${LIBCYPHAL_INCLUDE}/
    LIBRARIES
        canard
        udpard
        cetl
)

find_package(clangformat)

if (clangformat_FOUND)

# define a dry-run version that we always run.
enable_clang_format_check_for_directory(
    DIRECTORY ${LIBCYPHAL_INCLUDE}
    GLOB_PATTERN "**/*.hpp"
    ADD_TO_ALL
)

# provide an in-place format version as a helper that must be manually run.
enable_clang_format_check_for_directory(
    DIRECTORY ${LIBCYPHAL_INCLUDE}
    GLOB_PATTERN "**/*.hpp"
    FORMAT_IN_PLACE
)

endif()

# +---------------------------------------------------------------------------+
endif()
