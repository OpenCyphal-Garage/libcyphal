#
# Ensure clang-format is available and check style rules.
#

find_program(CLANG_FORMAT clang-format)


#
# :function: create_check_style_target
# Create a target that checks for compliance with code style rules.
#
# :param str ARG_STYLE_TARGET_NAME:  The name to give the target created by this function.
# :param bool ARG_ADD_TO_ALL:        If true the target is added to the default build target.
#
function(create_check_style_target ARG_STYLE_TARGET_NAME ARG_ADD_TO_ALL ARG_GLOB_PATTERN)

    add_custom_target(${ARG_STYLE_TARGET_NAME}-clang-format-check
                      COMMAND ${CMAKE_MODULE_PATH}/clang-format-check.py
                              --clang-format-path ${CLANG_FORMAT}
                              ${ARG_GLOB_PATTERN}
                      VERBATIM
                      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    )

    if  (ARG_ADD_TO_ALL)
        add_custom_target(${ARG_STYLE_TARGET_NAME} ALL DEPENDS ${ARG_STYLE_TARGET_NAME}-clang-format-check)
    else()
        add_custom_target(${ARG_STYLE_TARGET_NAME} DEPENDS ${ARG_STYLE_TARGET_NAME}-clang-format-check)
    endif()

endfunction(create_check_style_target)

#
# :function: create_apply_style_target
# Create a target that reformats source, in-place, based on formatting rules.
#
# :param str ARG_STYLE_TARGET_NAME:  The name to give the target created by this function.
# :param bool ARG_ADD_TO_ALL:        If true the target is added to the default build target.
# :param List[str] ...:              A list of files to format.
#
function(create_apply_style_target ARG_STYLE_TARGET_NAME ARG_ADD_TO_ALL)

    set(LOCAL_REFORMAT_FILES "")

    if (${ARGC} GREATER 2)
        foreach(ARG_N RANGE 2 ${ARGC}-1)
            list(APPEND LOCAL_REFORMAT_FILES ${ARGV${ARG_N}})
        endforeach(ARG_N)
    endif()

    add_custom_target( ${ARG_STYLE_TARGET_NAME}-inplace
                       COMMAND ${CLANG_FORMAT} -style=file
                                               -i
                                               ${LOCAL_REFORMAT_FILES}
                       DEPENDS ${LOCAL_REFORMAT_FILES}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    )

    if  (ARG_ADD_TO_ALL)
        add_custom_target(${ARG_STYLE_TARGET_NAME} ALL DEPENDS ${ARG_STYLE_TARGET_NAME}-inplace)
    else()
        add_custom_target(${ARG_STYLE_TARGET_NAME} DEPENDS ${ARG_STYLE_TARGET_NAME}-inplace)
    endif()

endfunction(create_apply_style_target)


include(FindPackageHandleStandardArgs)
 
find_package_handle_standard_args(clangformat
    REQUIRED_VARS CLANG_FORMAT
)
