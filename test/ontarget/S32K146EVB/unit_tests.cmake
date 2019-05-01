#
# Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
#

set(BOARD_NAME S32K146EVB)
set(MCU_MANUFACTURER NXP)
set(MCU_FAMILY S32K1)
set(MCU_LINE S32K146)

set(ONTARGET_TEST_PATH ${CMAKE_SOURCE_DIR}/test/ontarget)

add_definitions(-DCPU_${MCU_LINE}
                -DDISABLE_WDOG
)

include_directories(
    ${ONTARGET_TEST_PATH}/${BOARD_NAME}/include
    ${ONTARGET_TEST_PATH}/CMSIS/Core/Include
)

set(MCU_LINKER_SCRIPT "${ONTARGET_TEST_PATH}/${BOARD_NAME}/Project_Settings/Linker_Files/${MCU_FAMILY}xx_flash.ld")

set(USER_SOURCES "${ONTARGET_TEST_PATH}/${BOARD_NAME}/src/main.cpp")

set(BSP_SOURCES "${ONTARGET_TEST_PATH}/${BOARD_NAME}/Project_Settings/Startup_Code/startup_${MCU_LINE}.S"
                "${ONTARGET_TEST_PATH}/${BOARD_NAME}/Project_Settings/Startup_Code/system_${MCU_LINE}.c"
                "${ONTARGET_TEST_PATH}/${BOARD_NAME}/Project_Settings/Startup_Code/startup.c"
)

set(SOURCE_FILES ${USER_SOURCES} ${BSP_SOURCES})

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T ${MCU_LINKER_SCRIPT}")

# +---------------------------------------------------------------------------+
# | BUILD TESTS ON-TARGET TESTING
# +---------------------------------------------------------------------------+
file(GLOB_RECURSE TEST_CXX_FILES 
     RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} 
     "test/native/*.cpp")

add_executable(${PROJECT_NAME}.elf ${TEST_CXX_FILES} ${SOURCE_FILES})

add_dependencies(${PROJECT_NAME}.elf dsdl-regulated)

set_target_properties(${PROJECT_NAME}.elf
                      PROPERTIES
                      LINK_DEPENDS ${MCU_LINKER_SCRIPT})

target_link_libraries(${PROJECT_NAME}.elf gmock_main)

set(HEX_FILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex)
set(BIN_FILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
                   COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${HEX_FILE}
                   COMMENT "elf -> ${HEX_FILE}")

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
                   COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${BIN_FILE}
                   COMMENT "elf -> ${BIN_FILE}")
