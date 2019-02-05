#
# Pull the public_regulated_data_types.
#

configure_file(${CMAKE_MODULE_PATH}/public_regulated_data_types.txt.in ${EXTERNAL_PROJECT_DIRECTORY}/public-regulated-data-types-download/CMakeLists.txt)

execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE DSDL_CMAKE_GEN_RESULT
    WORKING_DIRECTORY ${EXTERNAL_PROJECT_DIRECTORY}/public-regulated-data-types-download )

if(DSDL_CMAKE_GEN_RESULT)
    message(WARNING "CMake step for public, regulated data types failed: ${DSDL_CMAKE_GEN_RESULT}")
else()
    execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE DSDL_CMAKE_BUILD_RESULT
        WORKING_DIRECTORY ${EXTERNAL_PROJECT_DIRECTORY}/public-regulated-data-types-download)

    if(DSDL_CMAKE_BUILD_RESULT)
        message(WARNING "Build step for public, regulated data types failed: ${DSDL_CMAKE_BUILD_RESULT}")
    else()

        set(PUBLIC_REGULATED_DATA_TYPES_FOUND ON)
    endif()
endif()

