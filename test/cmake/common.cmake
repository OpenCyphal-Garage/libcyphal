#
# Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# Common stuff to include in all libcyphal CMakeLists.txt
#

if (NOT DEFINED LIBCYPHAL_EXT_FOLDER)
    get_filename_component(LIBCYPHAL_BUILD_DIR_STUB ${CMAKE_CURRENT_BINARY_DIR} NAME)
    set(EXTERNAL_PROJECT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${LIBCYPHAL_BUILD_DIR_STUB}_ext")
else()
    set(EXTERNAL_PROJECT_DIRECTORY "${LIBCYPHAL_EXT_FOLDER}")
endif()

# +---------------------------------------------------------------------------+
# | Flag set handling
# +---------------------------------------------------------------------------+

function(apply_flag_set ARG_FLAG_SET)
    include(${ARG_FLAG_SET})

    # +-----------------------------------------------------------------------+
    # | CONFIGURABLE DEFINITIONS
    # +-----------------------------------------------------------------------+
    if(NOT DEFINED LIBCYPHAL_ENABLE_EXCEPTIONS AND "-fexceptions" IN_LIST CXX_FLAG_SET)
        set(LIBCYPHAL_ENABLE_EXCEPTIONS 1)
    endif()

    if(DEFINED LIBCYPHAL_ENABLE_EXCEPTIONS)
        list(APPEND C_FLAG_SET "-DLIBCYPHAL_ENABLE_EXCEPTIONS=${LIBCYPHAL_ENABLE_EXCEPTIONS}")
        list(APPEND CXX_FLAG_SET "-DLIBCYPHAL_ENABLE_EXCEPTIONS=${LIBCYPHAL_ENABLE_EXCEPTIONS}")
    endif()

    if(NOT DEFINED LIBCYPHAL_ENABLE_EXCEPTIONS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(LIBCYPHAL_INTROSPECTION_ENABLE_ASSERT 1)
    endif()

    if(DEFINED LIBCYPHAL_INTROSPECTION_ENABLE_ASSERT)
        list(APPEND C_FLAG_SET "-DLIBCYPHAL_INTROSPECTION_ENABLE_ASSERT=${LIBCYPHAL_INTROSPECTION_ENABLE_ASSERT}")
        list(APPEND CXX_FLAG_SET "-DLIBCYPHAL_INTROSPECTION_ENABLE_ASSERT=${LIBCYPHAL_INTROSPECTION_ENABLE_ASSERT}")
    endif()

    if(NOT DEFINED LIBCYPHAL_INTROSPECTION_TRACE_ENABLE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(LIBCYPHAL_INTROSPECTION_TRACE_ENABLE 1)
    endif()

    if(DEFINED LIBCYPHAL_INTROSPECTION_TRACE_ENABLE)
        list(APPEND C_FLAG_SET "-DLIBCYPHAL_INTROSPECTION_TRACE_ENABLE=${LIBCYPHAL_INTROSPECTION_TRACE_ENABLE}")
        list(APPEND CXX_FLAG_SET "-DLIBCYPHAL_INTROSPECTION_TRACE_ENABLE=${LIBCYPHAL_INTROSPECTION_TRACE_ENABLE}")
    endif()

    # +-----------------------------------------------------------------------+

    add_compile_options("$<$<COMPILE_LANGUAGE:C>:${C_FLAG_SET}>")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAG_SET}>")
    add_compile_options("$<$<COMPILE_LANGUAGE:ASM>:${ASM_FLAG_SET}>")
    add_link_options(${EXE_LINKER_FLAG_SET})
    add_definitions(${DEFINITIONS_SET})

endfunction()
