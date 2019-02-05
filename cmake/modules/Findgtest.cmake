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
configure_file(${CMAKE_MODULE_PATH}/Gtest.txt.in ${EXTERNAL_PROJECT_DIRECTORY}/googletest-download/CMakeLists.txt)

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
if(GTEST_USE_LOCAL_BUILD)
    # googletest doesn't support armv7m and/or no-sys builds. We have to build
    # this ourselves.

    set(GOOGLETEST_SUBMODULE "${EXTERNAL_PROJECT_DIRECTORY}/googletest-src")

    include_directories(
        ${GOOGLETEST_SUBMODULE}/googletest/include
        ${GOOGLETEST_SUBMODULE}/googlemock/include
    )

    add_library(gmock_main STATIC EXCLUDE_FROM_ALL
                ${GOOGLETEST_SUBMODULE}/googlemock/src/gmock-all.cc
                ${GOOGLETEST_SUBMODULE}/googletest/src/gtest-all.cc
    )

    target_include_directories(gmock_main PRIVATE
                               ${GOOGLETEST_SUBMODULE}/googletest
                               ${GOOGLETEST_SUBMODULE}/googlemock
    )

    #
    # These are all "namespaced" with GTEST and may be needed by gtest headers so
    # we're right to add them to the global set of build definitions.
    #
    add_definitions(-DGTEST_HAS_POSIX_RE=0 
                    -DGTEST_HAS_PTHREAD=0
                    -DGTEST_HAS_DEATH_TEST=0
                    -DGTEST_HAS_STREAM_REDIRECTION=0
                    -DGTEST_OS_NONE
                    -DGTEST_HAS_RTTI=0
                    -DGTEST_HAS_EXCEPTIONS=0
                    -DGTEST_HAS_DOWNCAST_=0
                    -DGTEST_HAS_MUTEX_AND_THREAD_LOCAL_=0
                    -DGTEST_USES_POSIX_RE=0
                    -DGTEST_USES_PCRE=0
                    -DGTEST_LANG_CXX11=1
                    -DGTEST_OS_WINDOWS=0
                    -DGTEST_OS_WINDOWS_DESKTOP=0
                    -DGTEST_OS_WINDOWS_MINGW=0
                    -DGTEST_OS_WINDOWS_RT=0
                    -DGTEST_OS_WINDOWS_MOBILE=0
                    -DGTEST_OS_WINDOWS_PHONE=0
                    -DGTEST_OS_LINUX_ANDROID=0
                    -DGTEST_OS_CYGWIN=0
                    -DGTEST_OS_QNX=0
                    -DGTEST_OS_MAC=0
                    -DGTEST_OS_AIX=0
                    -DGTEST_OS_HPUX=0
                    -DGTEST_OS_OPENBSD=0
                    -DGTEST_OS_FREEBSD=0
                    -DGTEST_OS_LINUX=0
                    -DGTEST_OS_SOLARIS=0
                    -DGTEST_OS_SYMBIAN=0
                    -DGTEST_LINKED_AS_SHARED_LIBRARY=0
                    -DGTEST_CREATE_SHARED_LIBRARY=0
                    -DGTEST_DONT_DEFINE_FAIL=0
                    -DGTEST_DONT_DEFINE_SUCCEED=0
                    -DGTEST_DONT_DEFINE_ASSERT_EQ=0
                    -DGTEST_DONT_DEFINE_ASSERT_NE=0
                    -DGTEST_DONT_DEFINE_ASSERT_GT=0
                    -DGTEST_DONT_DEFINE_ASSERT_LT=0
                    -DGTEST_DONT_DEFINE_ASSERT_GE=0
                    -DGTEST_DONT_DEFINE_ASSERT_LE=0
                    -DGTEST_DONT_DEFINE_TEST=0
    )

else()

    # This is a "native" build so just add googletest directly.
    # This defines the gmock_main target.
    add_subdirectory(${EXTERNAL_PROJECT_DIRECTORY}/googletest-src
                    ${EXTERNAL_PROJECT_DIRECTORY}/googletest-build
                    EXCLUDE_FROM_ALL)

endif()
endif()
