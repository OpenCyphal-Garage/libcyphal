#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

# +---------------------------------------------------------------------------+
# | BUILD NATIVE EXAMPLE CODE
# +---------------------------------------------------------------------------+
# All example binaries will be created under this directory.
set(CETLVAST_NATIVE_EXAMPLE_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/cetlvast/suites/examples)


#
# function: define_native_example_build - creates an rule to build an executable for
# a given example.
#
# param: ARG_EXAMPLE_NAME   string     - The name to give the executable binary.
# param: ARG_EXAMPLE_SOURCE List[path] - A list of source files to compile into
#                                        the example binary.
# param: ARG_OUTDIR path               - A path to output example binaries.
#
function(define_native_example_build ARG_EXAMPLE_NAME ARG_EXAMPLE_SOURCE ARG_OUTDIR)

     add_executable(${ARG_EXAMPLE_NAME} ${ARG_EXAMPLE_SOURCE})
     set_target_properties(${ARG_EXAMPLE_NAME}
                           PROPERTIES
                           RUNTIME_OUTPUT_DIRECTORY "${ARG_OUTDIR}"
     )

endfunction()


#
# function: define_native_example_run - creates a rule that will and run individual
# examples.
#
# param: ARG_EXAMPLE_NAME string - The name of the example to run. A target will be created
#                                  with the name run_${ARG_EXAMPLE_NAME}
# param: ARG_OUTDIR path         - The path where the example binaries live.
#
function(define_native_example_run ARG_EXAMPLE_NAME ARG_OUTDIR)
     add_custom_target(
          run_${ARG_EXAMPLE_NAME}
          COMMAND
               ${ARG_OUTDIR}/${ARG_EXAMPLE_NAME}
          DEPENDS
               ${ARG_EXAMPLE_NAME}
     )

endfunction()

file(GLOB NATIVE_EXAMPLES
     LIST_DIRECTORIES false
     RELATIVE ${CETLVAST_PROJECT_ROOT}
        suites/docs/examples/example_*.cpp
)

set(ALL_EXAMPLES "")
set(ALL_EXAMPLE_RUNS "")

foreach(NATIVE_EXAMPLE ${NATIVE_EXAMPLES})
    cmake_path(GET NATIVE_EXAMPLE STEM NATIVE_EXAMPLE_NAME)
    message(STATUS "Defining native example binary ${NATIVE_EXAMPLE_NAME} for source file ${NATIVE_EXAMPLE}")
    define_native_example_build(${NATIVE_EXAMPLE_NAME} ${NATIVE_EXAMPLE} ${CETLVAST_NATIVE_EXAMPLE_BINARY_DIR})
    define_native_example_run(${NATIVE_EXAMPLE_NAME} ${CETLVAST_NATIVE_EXAMPLE_BINARY_DIR})
    list(APPEND ALL_EXAMPLES "${NATIVE_EXAMPLE_NAME}")
    list(APPEND ALL_EXAMPLE_RUNS "run_${NATIVE_EXAMPLE_NAME}")
endforeach()

add_custom_target(
     build_all
     DEPENDS
          ${ALL_EXAMPLES}
)

add_custom_target(
     run_all_examples
     DEPENDS
          ${ALL_EXAMPLE_RUNS}
)

# +---------------------------------------------------------------------------+
# | DOXYGEN
# +---------------------------------------------------------------------------+
#
# Finds programs needed to build the CETL documentation
#
find_package(docs REQUIRED)

create_docs_target(docs ON ${CETLVAST_PROJECT_ROOT}/suites/docs/examples "${ALL_EXAMPLES}")
create_docs_tarball_target(docs_tarball OFF)

add_custom_target(
     suite_all
     COMMENT
        "All CETL suites define this target as a default action scripts can rely on."
     DEPENDS
        docs
)
