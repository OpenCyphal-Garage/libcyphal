#
# Find nnvg and setup python environment to generate C++ from DSDL.
# Copyright 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#

# +---------------------------------------------------------------------------+
# | BUILD FUNCTIONS
# +---------------------------------------------------------------------------+
#
# :function: create_dsdl_target
# Creates a target that will generate source code from dsdl definitions.
#
# Extra command line arguments can be passed to nnvg by setting the string variable NNVG_FLAGS.
#
# :param str ARG_TARGET_NAME:               The name to give the target.
# :param str ARG_OUTPUT_LANGUAGE            The language to generate for this target.
# :param str ARG_OUTPUT_LANGUAGE_STD        The language standard use.
# :param Path ARG_OUTPUT_FOLDER:            The directory to generate all source under.
# :param Path ARG_DSDL_ROOT_DIR:            A directory containing the root namespace dsdl.
# :param bool ARG_ENABLE_CLANG_FORMAT:      If ON then clang-format will be run on each generated file.
# :param bool ARG_ENABLE_SER_ASSERT:        Generates code with serialization asserts enabled
# :param bool ARG_DISABLE_SER_FP:           Generates code with floating point support removed from
#                                           serialization logic.
# :param bool ARG_ENABLE_OVR_VAR_ARRAY:     Generates code with variable array capacity override enabled
# :param bool ARG_ENABLE_EXPERIMENTAL:      If true then nnvg is invoked with support for experimental
#                                           languages.
# :param str ARG_SER_ENDIANNESS:            One of 'any', 'big', or 'little' to pass as the value of the
#                                           nnvg `--target-endianness` argument. Set to an empty string
#                                           to omit this argument.
# :param str ARG_GENERATE_SUPPORT:          value for the nnvg --generate-support argument. See
#                                           nnvg --help for documentation
# :param ...:                               A list of paths to use when looking up dependent DSDL types.
# :return: Sets a variable "ARG_TARGET_NAME"-OUTPUT in the parent scope to the list of files the target
#           will generate. For example, if ARG_TARGET_NAME == 'foo-bar' then after calling this function
#           ${foo-bar-OUTPUT} will be set to the list of output files.
#
function (create_dsdl_target ARG_TARGET_NAME
                             ARG_OUTPUT_LANGUAGE
                             ARG_OUTPUT_LANGUAGE_STD
                             ARG_OUTPUT_FOLDER
                             ARG_DSDL_ROOT_DIR
                             ARG_ENABLE_CLANG_FORMAT
                             ARG_ENABLE_SER_ASSERT
                             ARG_DISABLE_SER_FP
                             ARG_ENABLE_OVR_VAR_ARRAY
                             ARG_ENABLE_EXPERIMENTAL
                             ARG_SER_ENDIANNESS
                             ARG_GENERATE_SUPPORT)

    separate_arguments(NNVG_CMD_ARGS UNIX_COMMAND "${NNVG_FLAGS}")

    if (${ARGC} GREATER 12)
        MATH(EXPR ARG_N_LAST "${ARGC}-1")
        foreach(ARG_N RANGE 12 ${ARG_N_LAST})
            list(APPEND NNVG_CMD_ARGS "-I")
            list(APPEND NNVG_CMD_ARGS "${ARGV${ARG_N}}")
        endforeach(ARG_N)
    endif()

    list(APPEND NNVG_CMD_ARGS --target-language)
    list(APPEND NNVG_CMD_ARGS ${ARG_OUTPUT_LANGUAGE})
    list(APPEND NNVG_CMD_ARGS  -O)
    list(APPEND NNVG_CMD_ARGS ${ARG_OUTPUT_FOLDER})
    list(APPEND NNVG_CMD_ARGS ${ARG_DSDL_ROOT_DIR})

    if (NOT "${ARG_SER_ENDIANNESS}" STREQUAL "")
        list(APPEND NNVG_CMD_ARGS "--target-endianness")
        list(APPEND NNVG_CMD_ARGS ${ARG_SER_ENDIANNESS})
        message(STATUS "nnvg:Setting --target-endianness to ${ARG_SER_ENDIANNESS}")
    endif()

    if (NOT "${ARG_OUTPUT_LANGUAGE_STD}" STREQUAL "")
        list(APPEND NNVG_CMD_ARGS "-std")
        list(APPEND NNVG_CMD_ARGS ${ARG_OUTPUT_LANGUAGE_STD})
        message(STATUS "nnvg:Setting -std to ${ARG_OUTPUT_LANGUAGE_STD}")
    endif()

    if (ARG_ENABLE_SER_ASSERT)
        list(APPEND NNVG_CMD_ARGS "--enable-serialization-asserts")
        message(STATUS "nnvg:Enabling seralization asserts in generated code.")
    endif()

    if (ARG_DISABLE_SER_FP)
        list(APPEND NNVG_CMD_ARGS "--omit-float-serialization-support")
        message(STATUS "nnvg:Disabling floating point seralization routines in generated support code.")
    endif()

    if (ARG_ENABLE_OVR_VAR_ARRAY)
        list(APPEND NNVG_CMD_ARGS "--enable-override-variable-array-capacity")
        message(STATUS "nnvg:Enabling variable array capacity override option in generated code.")
    endif()

    if (ARG_ENABLE_EXPERIMENTAL)
        list(APPEND NNVG_CMD_ARGS "--experimental-languages")
        message(STATUS "nnvg:Enabling support for experimental languages.")
    endif()

    execute_process(COMMAND ${NNVG} --generate-support=${ARG_GENERATE_SUPPORT} --list-outputs ${NNVG_CMD_ARGS}
                    OUTPUT_VARIABLE OUTPUT_FILES
                    RESULT_VARIABLE LIST_OUTPUTS_RESULT)

    if(NOT LIST_OUTPUTS_RESULT EQUAL 0)
        message(FATAL_ERROR "nnvg:Failed to retrieve a list of headers nnvg would "
                            "generate for the ${ARG_TARGET_NAME} target (${LIST_OUTPUTS_RESULT})"
                            " (${NNVG})")
    endif()

    execute_process(COMMAND ${NNVG} --generate-support=${ARG_GENERATE_SUPPORT} --list-inputs ${NNVG_CMD_ARGS}
                    OUTPUT_VARIABLE INPUT_FILES
                    RESULT_VARIABLE LIST_INPUTS_RESULT)

    if(NOT LIST_INPUTS_RESULT EQUAL 0)
        message(FATAL_ERROR "nnvg:Failed to resolve inputs using nnvg for the ${ARG_TARGET_NAME} "
                            "target (${LIST_INPUTS_RESULT})"
                            " (${NNVG})")
    endif()

    if(ARG_ENABLE_CLANG_FORMAT AND CLANG_FORMAT)
        set(CLANG_FORMAT_ARGS -pp-rp=${CLANG_FORMAT} -pp-rpa=-i -pp-rpa=-style=file)
    else()
        set(CLANG_FORMAT_ARGS "")
    endif()

    add_custom_command(OUTPUT ${OUTPUT_FILES}
                       COMMAND ${NNVG} --generate-support=${ARG_GENERATE_SUPPORT} ${CLANG_FORMAT_ARGS} ${NNVG_CMD_ARGS}
                       DEPENDS ${INPUT_FILES}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                       COMMENT "Running nnvg")

    add_custom_target(${ARG_TARGET_NAME}-gen
                      DEPENDS ${OUTPUT_FILES})

    add_library(${ARG_TARGET_NAME} INTERFACE)

    add_dependencies(${ARG_TARGET_NAME} ${ARG_TARGET_NAME}-gen)

    target_include_directories(${ARG_TARGET_NAME} INTERFACE ${ARG_OUTPUT_FOLDER})

    if (ARG_ENABLE_SER_ASSERT)
        if(${ARG_TARGET_NAME} STREQUAL "unity")
            target_compile_options(${ARG_TARGET_NAME} INTERFACE
                "-DNUNAVUT_ASSERT=TEST_ASSERT"
            )
        elseif(${ARG_TARGET_NAME} STREQUAL "gtest")
            target_compile_options(${ARG_TARGET_NAME} INTERFACE
                "-DNUNAVUT_ASSERT=ASSERT_TRUE"
            )
        else()
            target_compile_options(${ARG_TARGET_NAME} INTERFACE
                "-DNUNAVUT_ASSERT=assert"
            )
        endif()
    endif()

    set(${ARG_TARGET_NAME}-OUTPUT ${OUTPUT_FILES} PARENT_SCOPE)

endfunction(create_dsdl_target)

# +---------------------------------------------------------------------------+
# | CONFIGURE: PYTHON ENVIRONMENT
# +---------------------------------------------------------------------------+

if(NOT VIRTUALENV_FOUND)

    message(STATUS "virtualenv was not found. You must have nunavut and its"
                   " dependencies available in the global python environment.")

    find_program(NNVG nnvg)

else()

    find_program(NNVG nnvg
                 PATHS ${VIRTUALENV_PYTHON_BIN}
                 NO_DEFAULT_PATH
    )

    if (NOT NNVG)
        message(WARNING "nnvg:nnvg program was not found. The build will probably fail. (${NNVG})")
    endif()
endif()

# +---------------------------------------------------------------------------+
# | CONFIGURE: VALIDATE NNVG
# +---------------------------------------------------------------------------+
if (NNVG)
    execute_process(COMMAND ${NNVG} --version
                    OUTPUT_VARIABLE NNVG_VERSION
                    RESULT_VARIABLE NNVG_VERSION_RESULT)

    if(NNVG_VERSION_RESULT EQUAL 0)
        string(STRIP ${NNVG_VERSION} NNVG_VERSION)
        message(STATUS "nnvg:${NNVG} --version: ${NNVG_VERSION}")
    endif()
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(nnvg
    REQUIRED_VARS NNVG
    VERSION_VAR NNVG_VERSION
)
