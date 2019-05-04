#
# Find dsdlgenj and setup python environment to generate C++ from DSDL.
#

# +---------------------------------------------------------------------------+
# | CONSTANTS
# +---------------------------------------------------------------------------+
set(DSDLGENJ_EXTENSION .hpp)
set(DSDLGENJ_MINIMUM_VERSION 1.0)

# +---------------------------------------------------------------------------+
# | BUILD FUNCTIONS
# +---------------------------------------------------------------------------+
#
# :function: create_dsdl_target
# Creates a target that will generate source code from dsdl definitions.
#
# The source is generated to files with ${DSDLGENJ_EXTENSION} as the extension.
#
# :param str ARG_TARGET_NAME:               The name to give the target.
# :param bool ARG_ADD_TO_ALL:               If true the target is added to the default build target.
# :param Path ARG_OUTPUT_FOLDER:            The directory to generate all source under.
# :param Path ARG_TEMPLATES_DIR:            A directory containing the templates to use to generate the source.
# :param Path ARG_DSDL_ROOT_DIR:            A directory containing the root namespace dsdl.
# :param ...:                               A list of paths to use when looking up dependent DSDL types.
# :returns: Sets a variable "ARG_TARGET_NAME"-OUTPUT in the parent scope to the list of files the target 
#           will generate. For example, if ARG_TARGET_NAME == 'foo-bar' then after calling this function
#           ${foo-bar-OUTPUT} will be set to the list of output files.
#
function (create_dsdl_target ARG_TARGET_NAME ARG_ADD_TO_ALL ARG_OUTPUT_FOLDER ARG_TEMPLATES_DIR ARG_DSDL_ROOT_DIR)

    set(LOOKUP_DIR_CMD_ARGS "")

    if (${ARGC} GREATER 5)
        foreach(ARG_N RANGE 5 ${ARGC}-1)
            list(APPEND LOOKUP_DIR_CMD_ARGS " -I ${ARGV${ARG_N}}")
        endforeach(ARG_N)
    endif()

    execute_process(COMMAND ${PYTHON} ${DSDLGENJ} 
                                        --list-outputs
                                        --output-extension ${DSDLGENJ_EXTENSION}
                                        -O ${ARG_OUTPUT_FOLDER}
                                        ${LOOKUP_DIR_CMD_ARGS}
                                        ${ARG_DSDL_ROOT_DIR}
                    OUTPUT_VARIABLE OUTPUT_FILES
                    RESULT_VARIABLE LIST_OUTPUTS_RESULT)

    if(NOT LIST_OUTPUTS_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to retrieve a list of headers dsdlgenj would "
                            "generate for the ${ARG_TARGET_NAME} target (${LIST_OUTPUTS_RESULT})"
                            " (${PYTHON} ${DSDLGENJ})")
    endif()

    execute_process(COMMAND ${PYTHON} ${DSDLGENJ} 
                                        --list-inputs
                                        -O ${ARG_OUTPUT_FOLDER}
                                        --templates ${ARG_TEMPLATES_DIR}
                                        ${LOOKUP_DIR_CMD_ARGS}
                                        ${ARG_DSDL_ROOT_DIR}
                    OUTPUT_VARIABLE INPUT_FILES
                    RESULT_VARIABLE LIST_INPUTS_RESULT)

    if(NOT LIST_INPUTS_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to resolve inputs using dsdlgenj for the ${ARG_TARGET_NAME} "
                            "target (${LIST_INPUTS_RESULT})"
                            " (${PYTHON} ${DSDLGENJ})")
    endif()

    add_custom_command(OUTPUT ${OUTPUT_FILES}
                       COMMAND ${PYTHON} ${DSDLGENJ} 
                                           --templates ${ARG_TEMPLATES_DIR}
                                           --output-extension ${DSDLGENJ_EXTENSION}
                                           -O ${ARG_OUTPUT_FOLDER}
                                           ${LOOKUP_DIR_CMD_ARGS}
                                           ${ARG_DSDL_ROOT_DIR}
                       DEPENDS ${INPUT_FILES}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                       COMMENT "Running dsdlgenj")

    if (ARG_ADD_TO_ALL)
        add_custom_target(${ARG_TARGET_NAME} ALL DEPENDS ${OUTPUT_FILES})
    else()
        add_custom_target(${ARG_TARGET_NAME} DEPENDS ${OUTPUT_FILES})
    endif()

    set(${ARG_TARGET_NAME}-OUTPUT ${OUTPUT_FILES} PARENT_SCOPE)

endfunction(create_dsdl_target)


# +---------------------------------------------------------------------------+
# | CONFIGURE: PYTHON ENVIRONMENT
# +---------------------------------------------------------------------------+
find_program(VIRTUALENV virtualenv)

if(NOT VIRTUALENV)

    message(STATUS "virtualenv was not found. You must have pydsdlgen and its"
                   " dependencies available in the global python environment.")

    find_program(DSDLGENJ dsdlgenj)

else()

    set(VIRTUALENV_OUTPUT ${EXTERNAL_PROJECT_DIRECTORY}/.pyenv)
    set(PYTHON_BIN ${VIRTUALENV_OUTPUT}/bin)
    set(PYTHON ${PYTHON_BIN}/python)
    set(PIP ${PYTHON} -m pip)
    set(PYTHON_REQUIREMENTS ${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt)

    if(NOT EXISTS ${VIRTUALENV_OUTPUT})
        message(STATUS "virtualenv found. Creating a virtual environment and installing requirements for build.")

        execute_process(COMMAND ${VIRTUALENV} -p python3 ${VIRTUALENV_OUTPUT}
                        WORKING_DIRECTORY ${EXTERNAL_PROJECT_DIRECTORY})
        #
        # Pypi: pull python dependencies from PyPi
        #
        # Pull packages we need to support our build and test environment.
        #
        execute_process(COMMAND ${PIP} --disable-pip-version-check --isolated install -r ${PYTHON_REQUIREMENTS}
                        WORKING_DIRECTORY ${EXTERNAL_PROJECT_DIRECTORY})
    else()
        message(STATUS "virtualenv ${VIRTUALENV_OUTPUT} exists. Not recreating (delete this directory to re-create).")
    endif()

    find_program(DSDLGENJ dsdlgenj HINTS ${PYTHON_BIN})

    if (NOT DSDLGENJ)
        message(WARNING "dsdlgenj program was not found. The build will probably fail. (${DSDLGENJ})")
    endif()
endif()

# +---------------------------------------------------------------------------+
# | CONFIGURE: VALIDATE DSDLGENJ
# +---------------------------------------------------------------------------+
if (DSDLGENJ)
    execute_process(COMMAND ${PYTHON} ${DSDLGENJ} --version
                    OUTPUT_VARIABLE DSDLGENJ_VERSION
                    RESULT_VARIABLE DSDLGENJ_VERSION_RESULT)

    if(DSDLGENJ_VERSION_RESULT EQUAL 0)
        message(STATUS "${PYTHON} ${DSDLGENJ} --version: ${DSDLGENJ_VERSION}")
    endif()
endif()

include(FindPackageHandleStandardArgs)
 
find_package_handle_standard_args(dsdlgenj
    REQUIRED_VARS DSDLGENJ_VERSION
)

if(DSDLGENJ_VERSION VERSION_LESS ${DSDLGENJ_MINIMUM_VERSION})
    message(FATAL_ERROR "dsdlgenj version ${DSDLGENJ_MINIMUM_VERSION} or greater required. ${DSDLGENJ_VERSION} found."
                        " you must update your version of dsdlgenj to build libuavcan.")
endif()
