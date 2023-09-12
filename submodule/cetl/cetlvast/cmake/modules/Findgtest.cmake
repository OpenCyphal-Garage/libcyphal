#
# Framework : Googletest
#
# (Taken from googletest/README.md documentation)
# GTest executable
# Download and unpack googletest at configure time. We don't
# model this as a submodule because this technique properly utilizes
# the googletest cmake build.
#

# +---------------------------------------------------------------------------+
# | PULL GTEST SOURCE
# +---------------------------------------------------------------------------+
configure_file(${CMAKE_MODULE_PATH}/Gtest.txt.in
               ${EXTERNAL_PROJECT_DIRECTORY}/googletest-download/CMakeLists.txt)

execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
   RESULT_VARIABLE GTEST_CMAKE_GEN_RESULT
   WORKING_DIRECTORY ${EXTERNAL_PROJECT_DIRECTORY}/googletest-download )

if(GTEST_CMAKE_GEN_RESULT)
    message(WARNING "CMake step for googletest failed: ${GTEST_CMAKE_GEN_RESULT}")
else()
    execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE GTEST_CMAKE_BUILD_RESULT
        WORKING_DIRECTORY ${EXTERNAL_PROJECT_DIRECTORY}/googletest-download)

    if(GTEST_CMAKE_BUILD_RESULT)
        message(WARNING "Build step for googletest failed: ${GTEST_CMAKE_BUILD_RESULT}")
    else()

        # Prevent overriding the parent project's compiler/linker
        # settings on Windows
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

        set(GTEST_FOUND ON)
        set(BUILD_TESTING ON)
        enable_testing()
    endif()
endif()


# +---------------------------------------------------------------------------+
# | gmock_main
# +---------------------------------------------------------------------------+
if(GTEST_FOUND)

    # This is a "native" build so just add googletest directly.
    # This defines the gmock_main target.
    add_subdirectory(${EXTERNAL_PROJECT_DIRECTORY}/googletest-src
                     ${EXTERNAL_PROJECT_DIRECTORY}/googletest-build
                     EXCLUDE_FROM_ALL)

    # Aparently Google doesn't care much about compiler warnings?
    set(LOCAL_GTEST_COMPILE_OPTIONS
        "-Wno-sign-conversion"
        "-Wno-zero-as-null-pointer-constant"
        "-Wno-switch-enum"
        "-Wno-float-equal"
        "-Wno-double-promotion"
        "-Wno-conversion"
        "-Wno-missing-declarations"
    )
    target_compile_options(gtest
        PRIVATE
            ${LOCAL_GTEST_COMPILE_OPTIONS}
    )
    target_compile_options(gmock
        PRIVATE
            ${LOCAL_GTEST_COMPILE_OPTIONS}
    )

endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(gtest
    REQUIRED_VARS GTEST_FOUND
)
