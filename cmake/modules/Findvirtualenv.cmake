#
# Find virtualenv. If found provide a way to setup a virtualenv for the build.
#

find_program(VIRTUALENV virtualenv)

if(VIRTUALENV)

    set(VIRTUALENV_OUTPUT ${EXTERNAL_PROJECT_DIRECTORY}/.pyenv)
    set(VIRTUALENV_PYTHON_BIN ${VIRTUALENV_OUTPUT}/bin)
    set(PYTHON ${VIRTUALENV_PYTHON_BIN}/python)
    set(PIP ${PYTHON} -m pip)
    set(PYTHON_REQUIREMENTS ${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt)

    if(NOT EXISTS ${VIRTUALENV_OUTPUT})
        message(STATUS "virtualenv found. Creating a virtual environment and installing core requirements.")

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

endif()

include(FindPackageHandleStandardArgs)
 
find_package_handle_standard_args(nnvg
    REQUIRED_VARS VIRTUALENV VIRTUALENV_PYTHON_BIN
)
