#
# Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#

# +---------------------------------------------------------------------------+
# | BUILD LINUX UNIT TESTS AND EXAMPLES
# +---------------------------------------------------------------------------+

#
# function: define_native_unit_test - creates an executable target and links it
# to the "all" target to build a gtest binary for the given test source.
#
# param: ARG_EXAMPLE_NAME string - The name to give the example binary.
# param: ARG_EXAMPLE_SOURCE List[path] - A list of source files to compile into
#                               the example binary.
# param: ARG_OUTDIR path - A path to output example binaries under.
#
function(define_native_example ARG_EXAMPLE_NAME ARG_EXAMPLE_SOURCE ARG_OUTDIR)

     add_executable(${ARG_EXAMPLE_NAME} ${ARG_EXAMPLE_SOURCE})
     target_link_libraries(${ARG_TEST_NAME} dsdl-regulated)

     set_target_properties(${ARG_EXAMPLE_NAME}
                           PROPERTIES
                           RUNTIME_OUTPUT_DIRECTORY "${ARG_OUTDIR}"
     )

endfunction()


define_native_example("media_on_linux"
                      ${CMAKE_CURRENT_SOURCE_DIR}/test/linux/example/media_on_linux/main.cpp
                      ${CMAKE_CURRENT_BINARY_DIR}/tests/linux/example
                    )
