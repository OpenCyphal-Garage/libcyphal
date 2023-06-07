#
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.22.0)

# +---------------------------------------------------------------------------+
# | PROJECT
# +---------------------------------------------------------------------------+

project(libcyphal_docs C CXX)

include("../cmake/CMakeCommon.cmake" REQUIRED)

add_subdirectory(examples)

# +---------------------------------------------------------------------------+
# | DOXYGEN
# +---------------------------------------------------------------------------+

find_package(Doxygen REQUIRED)
find_program(TAR tar)

# :function: create_docs_target
# Create a target that generates documentation.
#
# :param str ARG_DOCS_DOXY_ROOT:            The folder under which all doxygen configuration, templates,
#                                           styles, and examples can be found.
# :param str ARG_OPEN_CYPHAL_PROJECT_NAME:  The name of the org.opencyphal project this documentation is for.
# :param str ARG_PROJECT_VERSION            A string that contains the version of the cyphal project to display
#                                           in the generated documentation.
# :param str ARG_OPEN_CYPHAL_PROJECT_BRIEF: A one-line description of the project to be included in the generated
#                                           documents.
# :param bool ARG_W_ERROR                   If true then warnings are treated as errors.
# :param path ARG_EXAMPLES_PATH:            A path to example code to be included in the docs.
# :param path ARG_MAIN_PAGE_PATH:           A file to use as the main page (e.g. ${PROJECT_ROOT}/README.md).
# :param list[path] ARG_INPUT_LIST          A list of source files to generate documentation for.
# :param list[str] ARG_DEPENDS:             A list of dependencies the docs build should take.
#
# :return : A build target named ${ARG_OPEN_CYPHAL_PROJECT_NAME}_docs is defined.
#
function (create_docs_target ARG_DOCS_DOXY_ROOT
                             ARG_OPEN_CYPHAL_PROJECT_NAME
                             ARG_PROJECT_VERSION
                             ARG_OPEN_CYPHAL_PROJECT_BRIEF
                             ARG_W_ERROR
                             ARG_EXAMPLES_PATH
                             ARG_MAIN_PAGE_PATH
                             ARG_INPUT_LIST
                             ARG_DEPENDS)

    set(DOXYGEN_SOURCE ${ARG_DOCS_DOXY_ROOT})
    set(DOXYGEN_RDOMAIN org.opencyphal)
    set(DOXYGEN_RDOMAIN_W_PROJECT org.opencyphal.${ARG_OPEN_CYPHAL_PROJECT_NAME})
    set(DOXYGEN_PROJECT_NAME "${ARG_OPEN_CYPHAL_PROJECT_NAME}")
    set(DOXYGEN_PROJECT_VERSION "${ARG_PROJECT_VERSION}")
    set(DOXYGEN_PROJECT_BRIEF "${ARG_OPEN_CYPHAL_PROJECT_BRIEF}")
    if (${ARG_W_ERROR})
        set(DOXYGEN_WARN_AS_ERROR "YES")
    else()
        set(DOXYGEN_WARN_AS_ERROR "NO")
    endif()
    set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set(DOXYGEN_CONFIG_FILE ${DOXYGEN_OUTPUT_DIRECTORY}/doxygen.config)
    set(DOXYGEN_EXAMPLE_PATH ${ARG_EXAMPLES_PATH})

    list(APPEND DOXYGEN_INPUT_LIST ${ARG_INPUT_LIST})
    list(JOIN DOXYGEN_INPUT_LIST "\\\n    " DOXYGEN_INPUT )
    set(DOXYGEN_MAINPAGE "${ARG_MAIN_PAGE_PATH}")
    set(DOXYGEN_OUTPUT_TAGFILE "${DOXYGEN_OUTPUT_DIRECTORY}/${ARG_OPEN_CYPHAL_PROJECT_NAME}.tag.xml")
    set(DOXYGEN_COMPILE_COMMANDS_DIR ${CMAKE_BINARY_DIR})

    # +-----------------------------------------------------------------------+
    # | HTML
    # +-----------------------------------------------------------------------+
    set(DOXYGEN_HTML_EXTRA_FILES "${DOXYGEN_SOURCE}/.nojekyll")
    set(DOXYGEN_HTML_STYLESHEET ${DOXYGEN_OUTPUT_DIRECTORY}/stylesheet.css)
    set(DOXYGEN_HTML_EXTRA_STYLESHEET ${DOXYGEN_OUTPUT_DIRECTORY}/doxygen-flat-style.css)
    set(DOXYGEN_HTML_HEADER ${DOXYGEN_OUTPUT_DIRECTORY}/header.html)
    set(DOXYGEN_HTML_FOOTER ${DOXYGEN_OUTPUT_DIRECTORY}/footer.html)
    set(DOXYGEN_IMAGE_PATH ${DOXYGEN_SOURCE}/images)
    set(DOXYGEN_LOGO ${DOXYGEN_SOURCE}/images/html/opencyphal_logo.svg)
    set(DOXYGEN_TAGFILES "${DOXYGEN_SOURCE}/cppreference-doxygen-web.tag.xml=http://en.cppreference.com/w/")

    # a bit of a hack, but we copy all the images to the output to allow us to serve them
    # for anything other that the docs that wants them.
    file(COPY ${DOXYGEN_SOURCE}/images/html DESTINATION ${DOXYGEN_OUTPUT_DIRECTORY})

    configure_file(${DOXYGEN_SOURCE}/header.html
                    ${DOXYGEN_OUTPUT_DIRECTORY}/header.html
                )
    configure_file(${DOXYGEN_SOURCE}/footer.html
                    ${DOXYGEN_OUTPUT_DIRECTORY}/footer.html
                )
    configure_file(${DOXYGEN_SOURCE}/stylesheet.css
                    ${DOXYGEN_OUTPUT_DIRECTORY}/stylesheet.css
                )
    configure_file(${DOXYGEN_SOURCE}/doxygen-flat-style.css
                    ${DOXYGEN_OUTPUT_DIRECTORY}/doxygen-flat-style.css
                )
    configure_file(${DOXYGEN_SOURCE}/doxygen.ini
                    ${DOXYGEN_CONFIG_FILE}
                )
    add_custom_command(OUTPUT ${DOXYGEN_OUTPUT_DIRECTORY}/html/index.html
                        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_CONFIG_FILE}
                        DEPENDS
                            ${ARG_DEPENDS}
                            ${DOXYGEN_CONFIG_FILE}
                            ${DOXYGEN_INPUT_LIST}
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        COMMENT "Generating intermediate documentation."
                    )
    add_custom_target(generate_${ARG_OPEN_CYPHAL_PROJECT_NAME}_docs DEPENDS ${DOXYGEN_OUTPUT_DIRECTORY}/html/index.html)

    if (TAR)
        add_custom_command(OUTPUT ${DOXYGEN_OUTPUT_DIRECTORY}/html.gz
                        COMMAND ${TAR} -vzcf html.gz ./html
                        DEPENDS ${DOXYGEN_OUTPUT_DIRECTORY}/html/index.html
                        WORKING_DIRECTORY ${DOXYGEN_OUTPUT_DIRECTORY}
                        COMMENT "Creating html tarball."
                        )

        add_custom_target(archive_${ARG_OPEN_CYPHAL_PROJECT_NAME}_docs DEPENDS ${DOXYGEN_OUTPUT_DIRECTORY}/html.gz)
    endif()

endfunction(create_docs_target)

file(GLOB_RECURSE DOXYGEN_INPUT_LIST
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    ${LIBCYPHAL_ROOT}/libcyphal/include/**/*.hpp
)

get_property(LOCAL_EXAMPLES DIRECTORY "examples" PROPERTY IN_BUILD_TESTS)

list(TRANSFORM LOCAL_EXAMPLES PREPEND "examples/")

create_docs_target( ${CMAKE_CURRENT_SOURCE_DIR}
                    "libcyphal"
                    ${LIBCYPHAL_VERSION}
                    "OpenCyphal stack in C++"
                    OFF
                    ${CMAKE_CURRENT_SOURCE_DIR}/examples
                    "${LIBCYPHAL_ROOT}/README.md"
                    "${DOXYGEN_INPUT_LIST}"
                    "${LOCAL_EXAMPLES}"
)
