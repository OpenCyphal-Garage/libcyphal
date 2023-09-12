# Copyright Amazon.com Inc., or its affiliates. All Rights Reserved.

if(DEFINED LIBCYPHAL_CMAKE_MODULE_PATH)
    message(DEBUG "LIBCYPHAL_CMAKE_MODULE_PATH was set to ${LIBCYPHAL_CMAKE_MODULE_PATH}. Will use this to override CMAKE_MODULE_PATH.")
    set(CMAKE_MODULE_PATH ${LIBCYPHAL_CMAKE_MODULE_PATH})
else()
    set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
    message(DEBUG "CMAKE_MODULE_PATH set to ${CMAKE_MODULE_PATH}")
endif()

if(DEFINED LIBCYPHAL_EXTERNAL_PROJECT_DIRECTORY)
    message(DEBUG "LIBCYPHAL_EXTERNAL_PROJECT_DIRECTORY was set to ${LIBCYPHAL_EXTERNAL_PROJECT_DIRECTORY}. Will use this to override EXTERNAL_PROJECT_DIRECTORY.")
    set(EXTERNAL_PROJECT_DIRECTORY ${LIBCYPHAL_EXTERNAL_PROJECT_DIRECTORY})
else()
    set(EXTERNAL_PROJECT_DIRECTORY "${CMAKE_SOURCE_DIR}")
    message(DEBUG "EXTERNAL_PROJECT_DIRECTORY set to ${EXTERNAL_PROJECT_DIRECTORY}")
endif()

message(STATUS
"[ globals ]------------------------------------------------\n\
    CMAKE_MODULE_PATH:          ${CMAKE_MODULE_PATH}\n\
    EXTERNAL_PROJECT_DIRECTORY: ${EXTERNAL_PROJECT_DIRECTORY}\n\
------------------------------------------------------------\n\
")

# +---------------------------------------------------------------------------+
# | CMAKE EXTERNAL PROJECTS CRAP
# |     This is a temporary hack to manually pull in dependencies while the
# | large-scale scaffolding work on libcyphal is going on. We'll find a way
# | to either use Brazil or conan once the rate of change has slowed.
# |
# | CHANGE PROCEDURE:
# | 1. update the sha and/or git repo in the *.txt.in file for the dependency
# | 2. manually run this as a function from the submodule directly:
# |
# |       cd submodule && brazil-build-tool-exec cmake -P cmake/functions/update_external_projects.cmake
# |
# | 3. git add and commit then open a code-review for the updates.
# |
# | To change the SHA to pull see the txt.in files under cmake/external.
# +---------------------------------------------------------------------------+

#
# Helper for `Findfoo.cmake` module implementation that uses ExternalProject_Add
# to pull a git repository that is a source-only set of dependencies.
# @param ARG_PROJECT_NAME              The name of the project.
# @param ARG_PROJECT_INCLUDE_RELPATH   A relative path from the base of the repo to
#                                      include files it provides.
# @param ARG_PROJECT_BELLWEATHER_FILE  ${ARG_PROJECT_INCLUDE_RELPATH}/${ARG_PROJECT_BELLWEATHER_FILE}
#                                      is checked for existence as a success/fail
#                                      criteria for the function.
# @return ${LOCAL_UPPERCASE_PROJECT_NAME}_FOUND is set in the parent scope to support
#         the cmake find_package_handle_standard_args protocol.
# @return ${LOCAL_UPPERCASE_PROJECT_NAME}_EXTERNAL_PROJECT_PATH is set, if the git pull
#         step succeeds, set to a fully resolved path to the project directory.
# @return ${LOCAL_UPPERCASE_PROJECT_NAME}_INCLUDE_PATH is set, if the git pull step
#         succeeds, set to a fully resolved path to the project's include directory.
#         This, typically, is ${LOCAL_UPPERCASE_PROJECT_NAME}_EXTERNAL_PROJECT_PATH/${ARG_PROJECT_INCLUDE_RELPATH}
#
function (create_source_external_project_adder ARG_PROJECT_NAME ARG_PROJECT_INCLUDE_RELPATH ARG_PROJECT_BELLWEATHER_FILE)

    # +---------------------------------------------------------------------------+
    # | ALL PATHS
    # +---------------------------------------------------------------------------+
    cmake_path(APPEND CMAKE_MODULE_PATH .. external ${ARG_PROJECT_NAME}.txt.in OUTPUT_VARIABLE LOCAL_PROJECT_CMAKE_MODULE_FILE)
    cmake_path(APPEND EXTERNAL_PROJECT_DIRECTORY build external ${ARG_PROJECT_NAME} OUTPUT_VARIABLE LOCAL_PROJECT_DIR_DOWNLOAD)
    cmake_path(APPEND LOCAL_PROJECT_DIR_DOWNLOAD CMakeLists.txt OUTPUT_VARIABLE LOCAL_PROJECT_CMAKELISTS_FILE)
    cmake_path(APPEND EXTERNAL_PROJECT_DIRECTORY ${ARG_PROJECT_NAME} OUTPUT_VARIABLE LOCAL_PROJECT_DIR_SOURCE)
    cmake_path(APPEND LOCAL_PROJECT_DIR_SOURCE ${ARG_PROJECT_INCLUDE_RELPATH} OUTPUT_VARIABLE LOCAL_PROJECT_INCLUDE_PATH)
    cmake_path(APPEND LOCAL_PROJECT_INCLUDE_PATH ${ARG_PROJECT_BELLWEATHER_FILE} OUTPUT_VARIABLE LOCAL_BELLWEATHER_FILE)
    string(TOUPPER ${ARG_PROJECT_NAME} LOCAL_UPPERCASE_PROJECT_NAME)

    # +---------------------------------------------------------------------------+
    # | PULL PROJECT SOURCE
    # +---------------------------------------------------------------------------+

    configure_file(${LOCAL_PROJECT_CMAKE_MODULE_FILE}
                   ${LOCAL_PROJECT_CMAKELISTS_FILE}
                  )

    execute_process(COMMAND ${CMAKE_COMMAND} .
                    RESULT_VARIABLE LOCAL_PROJECT_CMAKE_GEN_RESULT
                    WORKING_DIRECTORY ${LOCAL_PROJECT_DIR_DOWNLOAD}
                   )

    if(LOCAL_PROJECT_CMAKE_GEN_RESULT)
        message(WARNING "CMake step for ${ARG_PROJECT_NAME} failed: ${LOCAL_PROJECT_CMAKE_GEN_RESULT}")
    else()
        execute_process(COMMAND ${CMAKE_COMMAND} --build .
            RESULT_VARIABLE PROJECT_CMAKE_BUILD_RESULT
            WORKING_DIRECTORY ${LOCAL_PROJECT_DIR_DOWNLOAD})

        if(PROJECT_CMAKE_BUILD_RESULT)
            message(WARNING "Download step for ${ARG_PROJECT_NAME} failed: ${PROJECT_CMAKE_BUILD_RESULT}")
        else()
            set(${LOCAL_UPPERCASE_PROJECT_NAME}_INCLUDE_PATH ${LOCAL_PROJECT_INCLUDE_PATH} PARENT_SCOPE)
            set(${LOCAL_UPPERCASE_PROJECT_NAME}_EXTERNAL_PROJECT_PATH ${LOCAL_PROJECT_DIR_SOURCE} PARENT_SCOPE)
            if (NOT EXISTS ${LOCAL_BELLWEATHER_FILE})
                message(WARNING "When trying to find ${ARG_PROJECT_NAME}: \n"
                                "${LOCAL_UPPERCASE_PROJECT_NAME}_INCLUDE_PATH is set to ${LOCAL_PROJECT_INCLUDE_PATH} but it "
                                "does not contain ${ARG_PROJECT_BELLWEATHER_FILE}.")
            else()
                message(STATUS "${ARG_PROJECT_NAME} source downloaded successfully. Available at ${LOCAL_UPPERCASE_PROJECT_NAME}_INCLUDE_PATH")
                set(${LOCAL_UPPERCASE_PROJECT_NAME}_FOUND ON PARENT_SCOPE)
            endif()
        endif()
    endif()

endfunction()

find_package(cetl REQUIRED)
find_package(o1heap REQUIRED)
find_package(libcanard REQUIRED)
find_package(libudpard REQUIRED)
