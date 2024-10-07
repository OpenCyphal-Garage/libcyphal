#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

find_package(Python3 COMPONENTS Interpreter REQUIRED)

if (${Python3_VERSION_MINOR} LESS 8)
    message(FATAL_ERROR "nnvg:Python 3.8 or greater is required to run nnvg")
endif()

# +---------------------------------------------------------------------------+
# | CUSTOM PROPERTIES
# +---------------------------------------------------------------------------+
define_property(TARGET
    PROPERTY DSDL_INCLUDE_PATH
    BRIEF_DOCS "Stores the include path to DSDL used to generate a given library target."
    FULL_DOCS "Any target that generates source code from DSDL must have this property set on it "
              "as the path used to find the DSDL."
)

define_property(TARGET
    PROPERTY NNVG_INTERNAL_IS_SUPPORT
    BRIEF_DOCS "Used by the nnvg package to tag a target as being a special source library target."
    FULL_DOCS "Private to the nnvg package. Do not use."
)

# +---------------------------------------------------------------------------+
# | FUNCTIONS :: PRIVATE
# +---------------------------------------------------------------------------+
#
# Common logic for building up the command line arguments for nnvg.
#
function (_init_nnvg_command_args)
    #+-[input]----------------------------------------------------------------+
    set(options)
    set(singleValueArgs OUTPUT_FOLDER OUTPUT_VARIABLE SUPPORT_VALUE ENABLE_SER_ASSERT ENABLE_OVR_VAR_ARRAY)
    set(multiValueArgs)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    if (NOT ARG_OUTPUT_FOLDER)
        set(ARG_OUTPUT_FOLDER ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    #+-[body]-----------------------------------------------------------------+

    # Set up common args used for all invocations of nnvg
    list(APPEND LOCAL_NNVG_CMD_ARGS "--experimental-languages" "--target-language=cpp")
    list(APPEND LOCAL_NNVG_CMD_ARGS  "-O" ${ARG_OUTPUT_FOLDER})

    if (CMAKE_CXX_STANDARD STREQUAL "14")
        list(APPEND LOCAL_NNVG_CMD_ARGS "--language-standard=cetl++14-17")
    else ()
        list(APPEND LOCAL_NNVG_CMD_ARGS "--language-standard=c++${CMAKE_CXX_STANDARD}-pmr")
    endif ()
    # support files must be generated in a discrete rule to avoid multiple rules
    # trying to generate the same file.

    if (ARG_SUPPORT_VALUE)
        list(APPEND LOCAL_NNVG_CMD_ARGS "--generate-support" "${ARG_SUPPORT_VALUE}")
    endif()

    if (ARG_ENABLE_SER_ASSERT)
        list(APPEND LOCAL_NNVG_CMD_ARGS "--enable-serialization-asserts")
    endif()

    if (ARG_ENABLE_OVR_VAR_ARRAY)
        list(APPEND LOCAL_NNVG_CMD_ARGS "--enable-override-variable-array-capacity")
    endif()

    #+-[output]---------------------------------------------------------------+
    if (NOT ARG_OUTPUT_VARIABLE)
        message(FATAL_ERROR "nnvg:_init_nnvg_command_args:OUTPUT_VARIABLE must be set")
    endif()
    set(${ARG_OUTPUT_VARIABLE} ${LOCAL_NNVG_CMD_ARGS} PARENT_SCOPE)
endfunction()

#
# Common logic to define commands and targets for invoking nnvg.
#
function (_define_nnvg_rules)
    #+-[input]----------------------------------------------------------------+
    set(options)
    set(singleValueArgs TARGET DSDL_ROOT_DIR VERBOSE ADD_TO_ALL ENABLE_SER_ASSERT)
    set(multiValueArgs NNVG_CMD_ARGS DSDL_DEPENDENCIES INPUT_FILES OUTPUT_FILES)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    list(FIND ARG_NNVG_CMD_ARGS "-O" LOCAL_OUTPUT_ARG_INDEX)
    if (${LOCAL_OUTPUT_ARG_INDEX} EQUAL -1)
        message(FATAL_ERROR "nnvg:_define_nnvg_rules:NNVG_CMD_ARGS must contain an -O (output) argument")
    endif()

    math(EXPR LOCAL_OUTPUT_FOLDER_VALUE_INDEX "${LOCAL_OUTPUT_ARG_INDEX} + 1")
    list(GET ARG_NNVG_CMD_ARGS ${LOCAL_OUTPUT_FOLDER_VALUE_INDEX} LOCAL_OUTPUT_FOLDER)

    if (NOT LOCAL_OUTPUT_FOLDER)
        message(FATAL_ERROR "nnvg:_define_nnvg_rules: couldn't find an output folder in the in put args.")
    endif()

    if (ARG_ADD_TO_ALL)
        set(LOCAL_ADD_TO_ALL ALL)
    else()
        set(LOCAL_ADD_TO_ALL "")
    endif()

    if (ARG_VERBOSE)
        set(LOCAL_VERBOSE "-v")
    else()
        set(LOCAL_VERBOSE "")
    endif()

    #+-[body]-----------------------------------------------------------------+
    # Now we setup a proper rule that will invoke nnvg to generate the source code.
    add_custom_command(OUTPUT ${ARG_OUTPUT_FILES}
                       COMMAND ${NNVG} ${LOCAL_VERBOSE} ${ARG_NNVG_CMD_ARGS}
                       DEPENDS ${ARG_INPUT_FILES}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                       COMMENT "Running nnvg to generate headers for ${ARG_TARGET}")

    # -gen target allows manual execution of the generation
    # command and it allows us to add a rule to the "all" target if the
    # ADD_TO_ALL option is set.
    add_custom_target(${ARG_TARGET}-gen ${LOCAL_ADD_TO_ALL}
                      DEPENDS ${ARG_OUTPUT_FILES})

    # finally we define an interface library to contain the code generated
    # by the custom command.
    add_library(${ARG_TARGET} INTERFACE)
    add_dependencies(${ARG_TARGET} ${ARG_TARGET}-gen)
    target_include_directories(${ARG_TARGET} INTERFACE ${LOCAL_OUTPUT_FOLDER})

    if (ARG_ENABLE_SER_ASSERT)
        target_compile_options(${ARG_TARGET} INTERFACE
            "-DNUNAVUT_ASSERT=assert"
        )
    endif()
    #+-[output]---------------------------------------------------------------+
endfunction()

# +---------------------------------------------------------------------------+
# | FUNCTIONS :: PUBLIC
# +---------------------------------------------------------------------------+
#
# :function: define_nuanvut_support_target
# Creates a target that will generate c++ source code needed to support types
# generated by nnvg.
#
# :param str            TARGET:                   The name to give the target.
# :param path           OUTPUT_FOLDER:            The directory to generate all source under. Defaults to
#                                                 ${CMAKE_CURRENT_BINARY_DIR}
# :option               ENABLE_OVR_VAR_ARRAY:     Generates code with variable array capacity override enabled
# :option               ENABLE_SER_ASSERT:        Generates code with serialization asserts enabled
# :option               VERBOSE                   Enable verbose output from nnvg.
function (define_nunavut_support_target)
    #+-[input]----------------------------------------------------------------+
    set(options VERBOSE ENABLE_SER_ASSERT ENABLE_OVR_VAR_ARRAY)
    set(singleValueArgs TARGET OUTPUT_FOLDER)
    set(multiValueArgs)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    #+-[body]-----------------------------------------------------------------+
    _init_nnvg_command_args(OUTPUT_FOLDER ${ARG_OUTPUT_FOLDER}
                            ENABLE_SER_ASSERT ${ARG_ENABLE_SER_ASSERT}
                            ENABLE_OVR_VAR_ARRAY ${ARG_ENABLE_OVR_VAR_ARRAY}
                            SUPPORT_VALUE "only"
                            OUTPUT_VARIABLE LOCAL_NNVG_CMD_ARGS)

    execute_process(COMMAND ${NNVG} --list-outputs ${LOCAL_NNVG_CMD_ARGS}
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    OUTPUT_VARIABLE LOCAL_OUTPUT_FILES
                    RESULT_VARIABLE LOCAL_LIST_OUTPUTS_RESULT)

    if(NOT LOCAL_LIST_OUTPUTS_RESULT EQUAL 0)
        message(FATAL_ERROR "nnvg:Failed to retrieve a list of headers nnvg would "
                            "generate for the ${ARG_TARGET} target (${LOCAL_LIST_OUTPUTS_RESULT})"
                            " (${NNVG})")
    elseif(ARG_VERBOSE)
        message(DEBUG "nnvg:Resolved outputs for ${ARG_TARGET} target:\n${LOCAL_OUTPUT_FILES}")
    endif()

    _define_nnvg_rules(TARGET ${ARG_TARGET}
                       NNVG_CMD_ARGS ${LOCAL_NNVG_CMD_ARGS}
                       OUTPUT_FILES ${LOCAL_OUTPUT_FILES}
                       VERBOSE ${ARG_VERBOSE}
                       ENABLE_SER_ASSERT ${ARG_ENABLE_SER_ASSERT})

    set_property(TARGET ${ARG_TARGET} PROPERTY NNVG_INTERNAL_IS_SUPPORT TRUE)

    #+-[OUTPUT]---------------------------------------------------------------+
endfunction()

#
# :function: add_dsdl_cpp_codegen
# Creates a target that will generate c++ source code from dsdl definitions.
#
# :param str            TARGET:                   The name to give the target.
# :param path           OUTPUT_FOLDER:            The directory to generate all source under. Defaults to
#                                                 ${CMAKE_CURRENT_BINARY_DIR}
# :param list[target]   DSDL_DEPENDENCIES:        A list of targets, also created by the add_dsdl_cpp_codegen
#                                                 function, that this target depends on.
# :param path           DSDL_ROOT_DIR:            A directory containing the root namespace dsdl.
# :option               ENABLE_OVR_VAR_ARRAY:     Generates code with variable array capacity override enabled
# :option               ENABLE_SER_ASSERT:        Generates code with serialization asserts enabled
# :option               ADD_TO_ALL                Adds the target to the ALL target.
# :option               VERBOSE                   Enable verbose output from nnvg.
function (add_dsdl_cpp_codegen)
    #+-[input]----------------------------------------------------------------+
    set(options VERBOSE ENABLE_SER_ASSERT ENABLE_OVR_VAR_ARRAY ADD_TO_ALL)
    set(singleValueArgs TARGET OUTPUT_FOLDER DSDL_ROOT_DIR)
    set(multiValueArgs DSDL_DEPENDENCIES)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    if (ARG_ADD_TO_ALL)
        set(LOCAL_ADD_TO_ALL ALL)
    else()
        set(LOCAL_ADD_TO_ALL "")
    endif()

    #+-[body]-----------------------------------------------------------------+

    if (ARG_VERBOSE)
        set(LOCAL_VERBOSE "-v")
    else()
        set(LOCAL_VERBOSE "")
    endif()

    _init_nnvg_command_args(OUTPUT_FOLDER ${ARG_OUTPUT_FOLDER}
                            ENABLE_SER_ASSERT ${ARG_ENABLE_SER_ASSERT}
                            ENABLE_OVR_VAR_ARRAY ${ARG_ENABLE_OVR_VAR_ARRAY}
                            SUPPORT_VALUE "never"
                            OUTPUT_VARIABLE LOCAL_NNVG_CMD_ARGS)

    set(LOCAL_SUPPORT_TARGET)

    # Any dsdl targets provided must have the path to the dsdl files used to generate
    # the source set on it as DSDL_INCLUDE_PATH.
    foreach(LOCAL_DSDL_DEPENDENCY ${ARG_DSDL_DEPENDENCIES})

        # First see if this is our support target...
        get_property(LOCAL_DSDL_DEPENDENCY_IS_SUPPORT TARGET ${LOCAL_DSDL_DEPENDENCY} PROPERTY NNVG_INTERNAL_IS_SUPPORT)
        if (LOCAL_DSDL_DEPENDENCY_IS_SUPPORT)
            if (LOCAL_SUPPORT_TARGET)
                message(FATAL_ERROR "nnvg:Target ${LOCAL_DSDL_DEPENDENCY} is a support target and there is already a support target ${LOCAL_SUPPORT_TARGET}")
            endif()
            set(LOCAL_SUPPORT_TARGET ${LOCAL_DSDL_DEPENDENCY})
            continue()
        endif()

        # ...otherwise it must be a dsdl target
        get_property(LOCAL_DSDL_DEPENDENCY_INCLUDES TARGET ${LOCAL_DSDL_DEPENDENCY} PROPERTY DSDL_INCLUDE_PATH)
        if (NOT LOCAL_DSDL_DEPENDENCY_INCLUDES)
            message(FATAL_ERROR "nnvg:Target ${LOCAL_DSDL_DEPENDENCY} is not a DSDL target (does not have DSDL_INCLUDE_PATH set)")
        endif()
        foreach(LOCAL_DSDL_DEPENDENCY_INCLUDE ${LOCAL_DSDL_DEPENDENCY_INCLUDES})
            list(APPEND LOCAL_NNVG_CMD_ARGS "-I" ${LOCAL_DSDL_DEPENDENCY_INCLUDE})
        endforeach()
    endforeach()

    list(APPEND LOCAL_NNVG_CMD_ARGS ${ARG_DSDL_ROOT_DIR})

    message(TRACE "nnvg: LOCAL_NNVG_CMD_ARGS: ${LOCAL_NNVG_CMD_ARGS}")

    # Invoke nnvg to get a list of files it would generate. This is a bit like
    # using file(GLOB) in that it runs at configure time and will not pick up
    # files added or removed without reconfguring. It does setup a chain of rules
    # that ensure any changes to DSDL will trigger a regeneration of the source
    # code.
    execute_process(COMMAND ${NNVG} --list-outputs ${LOCAL_NNVG_CMD_ARGS}
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    OUTPUT_VARIABLE LOCAL_OUTPUT_FILES
                    RESULT_VARIABLE LOCAL_LIST_OUTPUTS_RESULT)

    if(NOT LOCAL_LIST_OUTPUTS_RESULT EQUAL 0)
        message(FATAL_ERROR "nnvg:Failed to retrieve a list of headers nnvg would "
                            "generate for the ${ARG_TARGET} target (${LOCAL_LIST_OUTPUTS_RESULT})"
                            " (${NNVG})")
    elseif(ARG_VERBOSE)
        list(JOIN LOCAL_OUTPUT_FILES "\n" LOCAL_OUTPUT_FILES_DEBUG_OUTPUT)
        message(DEBUG "nnvg:Resolved outputs for ${ARG_TARGET} target:\n${LOCAL_OUTPUT_FILES_DEBUG_OUTPUT}")
    endif()

    # Same here as above (i.e. file(GLOB)-like mechanism) but for the DSDL inputs.
    execute_process(COMMAND ${NNVG} --list-inputs ${LOCAL_NNVG_CMD_ARGS}
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    OUTPUT_VARIABLE LOCAL_INPUT_FILES
                    RESULT_VARIABLE LOCAL_LIST_INPUTS_RESULT)

    if(NOT LOCAL_LIST_INPUTS_RESULT EQUAL 0)
        message(FATAL_ERROR "nnvg:Failed to resolve inputs using nnvg for the ${ARG_TARGET} "
                            "target (${LOCAL_LIST_INPUTS_RESULT})"
                            " (${NNVG})")
    elseif(ARG_VERBOSE)
        list(JOIN LOCAL_INPUT_FILES "\n" LOCAL_INPUT_FILE_DEBUG_OUTPUT)
        message(DEBUG "nnvg:Resolved inputs for ${ARG_TARGET} target:\n${LOCAL_INPUT_FILE_DEBUG_OUTPUT}")
    endif()

    _define_nnvg_rules(TARGET ${ARG_TARGET}
                       NNVG_CMD_ARGS ${LOCAL_NNVG_CMD_ARGS}
                       OUTPUT_FILES ${LOCAL_OUTPUT_FILES}
                       INPUT_FILES ${LOCAL_INPUT_FILES}
                       ADD_TO_ALL ${LOCAL_ADD_TO_ALL}
                       VERBOSE ${ARG_VERBOSE}
                       ENABLE_SER_ASSERT ${ARG_ENABLE_SER_ASSERT})

    set_property(TARGET ${ARG_TARGET} PROPERTY DSDL_INCLUDE_PATH ${ARG_DSDL_ROOT_DIR})

    if (LOCAL_SUPPORT_TARGET)
        add_dependencies(${ARG_TARGET} ${LOCAL_SUPPORT_TARGET})
    endif()

    if (ARG_DSDL_DEPENDENCIES)
        add_dependencies(${ARG_TARGET} ${ARG_DSDL_DEPENDENCIES})
    endif()

    #+-[OUTPUT]---------------------------------------------------------------+
endfunction(add_dsdl_cpp_codegen)

# +---------------------------------------------------------------------------+
# | CONFIGURE: PYTHON ENVIRONMENT
# +---------------------------------------------------------------------------+

find_program(NNVG nnvg)


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
    REQUIRED_VARS NNVG_VERSION
)
