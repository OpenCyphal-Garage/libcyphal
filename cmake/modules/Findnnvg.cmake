#
# Find nnvg and setup python environment to generate C++ from DSDL.
#

# +---------------------------------------------------------------------------+
# | CONSTANTS
# +---------------------------------------------------------------------------+
set(NNVG_EXTENSION .hpp)
set(NNVG_MINIMUM_VERSION 0.1)

# +---------------------------------------------------------------------------+
# | BUILD FUNCTIONS
# +---------------------------------------------------------------------------+
#
# :function: create_dsdl_target
# Creates a target that will generate source code from dsdl definitions.
#
# The source is generated to files with ${NNVG_EXTENSION} as the extension.
#
# :param str ARG_TARGET_NAME:               The name to give the target.
# :param Path ARG_OUTPUT_FOLDER:            The directory to generate all source under.
# :param Path ARG_TEMPLATES_DIR:            A directory containing the templates to use to generate the source.
# :param Path ARG_DSDL_ROOT_DIR:            A directory containing the root namespace dsdl.
# :param bool ARG_ENABLE_CLANG_FORMAT:      If ON then clang-format will be run on each generated file.
# :param ...:                               A list of paths to use when looking up dependent DSDL types.
# :returns: Sets a variable "ARG_TARGET_NAME"-OUTPUT in the parent scope to the list of files the target 
#           will generate. For example, if ARG_TARGET_NAME == 'foo-bar' then after calling this function
#           ${foo-bar-OUTPUT} will be set to the list of output files.
#
function (create_dsdl_target ARG_TARGET_NAME ARG_OUTPUT_FOLDER ARG_TEMPLATES_DIR ARG_DSDL_ROOT_DIR ARG_ENABLE_CLANG_FORMAT)

    set(LOOKUP_DIR_CMD_ARGS "")

    if (${ARGC} GREATER 6)
        foreach(ARG_N RANGE 6 ${ARGC}-1)
            list(APPEND LOOKUP_DIR_CMD_ARGS " -I ${ARGV${ARG_N}}")
        endforeach(ARG_N)
    endif()

    execute_process(COMMAND ${PYTHON} ${NNVG} 
                                        --list-outputs
                                        --output-extension ${NNVG_EXTENSION}
                                        -O ${ARG_OUTPUT_FOLDER}
                                        ${LOOKUP_DIR_CMD_ARGS}
                                        ${ARG_DSDL_ROOT_DIR}
                    OUTPUT_VARIABLE OUTPUT_FILES
                    RESULT_VARIABLE LIST_OUTPUTS_RESULT)

    if(NOT LIST_OUTPUTS_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to retrieve a list of headers nnvg would "
                            "generate for the ${ARG_TARGET_NAME} target (${LIST_OUTPUTS_RESULT})"
                            " (${PYTHON} ${NNVG})")
    endif()

    execute_process(COMMAND ${PYTHON} ${NNVG} 
                                        --list-inputs
                                        -O ${ARG_OUTPUT_FOLDER}
                                        --templates ${ARG_TEMPLATES_DIR}
                                        ${LOOKUP_DIR_CMD_ARGS}
                                        ${ARG_DSDL_ROOT_DIR}
                    OUTPUT_VARIABLE INPUT_FILES
                    RESULT_VARIABLE LIST_INPUTS_RESULT)

    if(NOT LIST_INPUTS_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to resolve inputs using nnvg for the ${ARG_TARGET_NAME} "
                            "target (${LIST_INPUTS_RESULT})"
                            " (${PYTHON} ${NNVG})")
    endif()

    if(ARG_ENABLE_CLANG_FORMAT AND CLANG_FORMAT)
        set(CLANG_FORMAT_ARGS -pp-rp=${CLANG_FORMAT} -pp-rpa=-i -pp-rpa=-style=file)
    else()
        set(CLANG_FORMAT_ARGS "")
    endif()

    add_custom_command(OUTPUT ${OUTPUT_FILES}
                       COMMAND ${PYTHON} ${NNVG} 
                                           --templates ${ARG_TEMPLATES_DIR}
                                           --output-extension ${NNVG_EXTENSION}
                                           -O ${ARG_OUTPUT_FOLDER}
                                           ${LOOKUP_DIR_CMD_ARGS}
                                           ${CLANG_FORMAT_ARGS}
                                           ${ARG_DSDL_ROOT_DIR}
                       DEPENDS ${INPUT_FILES}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                       COMMENT "Running nnvg")

    add_custom_target(${ARG_TARGET_NAME}-gen 
                      DEPENDS ${OUTPUT_FILES})

    add_library(${ARG_TARGET_NAME} INTERFACE)

    add_dependencies(${ARG_TARGET_NAME} ${ARG_TARGET_NAME}-gen)

    target_include_directories(${ARG_TARGET_NAME} INTERFACE ${ARG_OUTPUT_FOLDER})

    set(${ARG_TARGET_NAME}-OUTPUT ${OUTPUT_FILES} PARENT_SCOPE)

endfunction(create_dsdl_target)


# +---------------------------------------------------------------------------+
# | CONFIGURE: PYTHON ENVIRONMENT
# +---------------------------------------------------------------------------+

if(NOT VIRTUALENV)

    message(STATUS "virtualenv was not found. You must have nunavut and its"
                   " dependencies available in the global python environment.")

    find_program(NNVG nnvg)

else()

    find_program(NNVG nnvg HINTS ${VIRTUALENV_PYTHON_BIN})

    if (NOT NNVG)
        message(WARNING "nnvg program was not found. The build will probably fail. (${NNVG})")
    endif()
endif()

# +---------------------------------------------------------------------------+
# | CONFIGURE: VALIDATE NNVG
# +---------------------------------------------------------------------------+
if (NNVG)
    execute_process(COMMAND ${PYTHON} ${NNVG} --version
                    OUTPUT_VARIABLE NNVG_VERSION
                    RESULT_VARIABLE NNVG_VERSION_RESULT)

    if(NNVG_VERSION_RESULT EQUAL 0)
        message(STATUS "${PYTHON} ${NNVG} --version: ${NNVG_VERSION}")
    endif()
endif()

include(FindPackageHandleStandardArgs)
 
find_package_handle_standard_args(nnvg
    REQUIRED_VARS NNVG_VERSION
)

if(NNVG_VERSION VERSION_LESS ${NNVG_MINIMUM_VERSION})
    message(FATAL_ERROR "nnvg version ${NNVG_MINIMUM_VERSION} or greater required. ${NNVG_VERSION} found."
                        " you must update your version of nnvg to build libuavcan.")
endif()
