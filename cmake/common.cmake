#
# Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# Common stuff to include in all libuavcan CMakeLists.txt
#

if (NOT DEFINED LIBUAVCAN_EXT_FOLDER)
    set(EXTERNAL_PROJECT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build_ext")
else()
    set(EXTERNAL_PROJECT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${LIBUAVCAN_EXT_FOLDER}")
endif()

# +---------------------------------------------------------------------------+
# | Flag set handling
# +---------------------------------------------------------------------------+

function(apply_flag_set ARG_FLAG_SET)
    include(${ARG_FLAG_SET})

    # list(JOIN ) is a thing in cmake 3.12 but we only require 3.10.
    foreach(ITEM ${C_FLAG_SET})
        set(LOCAL_CMAKE_C_FLAGS "${LOCAL_CMAKE_C_FLAGS} ${ITEM}")
    endforeach()
    foreach(ITEM ${CXX_FLAG_SET})
        set(LOCAL_CMAKE_CXX_FLAGS "${LOCAL_CMAKE_CXX_FLAGS} ${ITEM}")
    endforeach()
    foreach(ITEM ${EXE_LINKER_FLAG_SET})
        set(LOCAL_CMAKE_EXE_LINKER_FLAGS "${LOCAL_CMAKE_EXE_LINKER_FLAGS} ${ITEM}")
    endforeach()
    foreach(ITEM ${ASM_FLAG_SET})
        set(LOCAL_CMAKE_ASM_FLAGS "${LOCAL_CMAKE_ASM_FLAGS} ${ITEM}")
    endforeach()

    set(CMAKE_C_FLAGS ${LOCAL_CMAKE_C_FLAGS} PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS ${LOCAL_CMAKE_CXX_FLAGS} PARENT_SCOPE)
    set(CMAKE_EXE_LINKER_FLAGS ${LOCAL_CMAKE_EXE_LINKER_FLAGS} PARENT_SCOPE)
    set(CMAKE_ASM_FLAGS ${LOCAL_CMAKE_ASM_FLAGS} PARENT_SCOPE)
    
    add_definitions(${DEFINITIONS_SET})
endfunction()
