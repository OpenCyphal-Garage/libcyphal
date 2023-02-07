#
# Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#

# +---------------------------------------------------------------------------+
# | BUILD LINUX UNIT TESTS AND EXAMPLES
# +---------------------------------------------------------------------------+

#
# function: define_native_unit_test - creates an executable target and links it
# to the "all" target to build a gtest binary for the given test source.
#
# :param str:       ARG_EXAMPLE_NAME - The name to give the example binary.
# :param Path:      ARG_OUTDIR path  - A path to output example binaries under.
# :param List[str]: ...              - Source files to compile into the example binary.
#
function(define_native_example ARG_EXAMPLE_NAME ARG_OUTDIR)

     set(LOCAL_SOURCE_FILES "")

     if (${ARGC} GREATER 2)
     foreach(ARG_N RANGE 2 ${ARGC}-1)
          list(APPEND LOCAL_SOURCE_FILES ${ARGV${ARG_N}})
     endforeach(ARG_N)
     endif()

     add_executable(${ARG_EXAMPLE_NAME} ${LOCAL_SOURCE_FILES})
     target_link_libraries(${ARG_TEST_NAME} dsdl-regulated)

     set_target_properties(${ARG_EXAMPLE_NAME}
                           PROPERTIES
                           RUNTIME_OUTPUT_DIRECTORY "${ARG_OUTDIR}"
     )

endfunction()


define_native_example("media_on_linux"
                      ${CMAKE_CURRENT_BINARY_DIR}/tests/linux/example
                      ${CMAKE_CURRENT_SOURCE_DIR}/linux/example/media_on_linux/main.cpp
                      ${CMAKE_CURRENT_SOURCE_DIR}/linux/example/media_on_linux/SocketCANInterface.cpp
                      ${CMAKE_CURRENT_SOURCE_DIR}/linux/example/media_on_linux/SocketCANInterfaceGroup.cpp
                      ${CMAKE_CURRENT_SOURCE_DIR}/linux/example/media_on_linux/SocketCANInterfaceManager.cpp
                    )
