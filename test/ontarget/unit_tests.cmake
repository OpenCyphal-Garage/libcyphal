#
# Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
#

set(BOARD_NAME nucleo_144)
set(MCU_MANUFACTURER ST)
set(MCU_FAMILY STM32H7)
set(MCU_FAMILY_LC stm32h7)
set(MCU_LINE STM32H743)
set(MCU_PART ${MCU_LINE}ZIT)

set(ONTARGET_TEST_PATH ${CMAKE_SOURCE_DIR}/test/ontarget)

include_directories(
    ${ONTARGET_TEST_PATH}/${BOARD_NAME}/include
    ${ONTARGET_TEST_PATH}/CMSIS/Device/${MCU_MANUFACTURER}/${MCU_FAMILY}xx/Include
    ${ONTARGET_TEST_PATH}/CMSIS/Include
    ${ONTARGET_TEST_PATH}/${BOARD_NAME}/Drivers/${MCU_FAMILY}xx_HAL_Driver/Inc
    ${ONTARGET_TEST_PATH}/${BOARD_NAME}/Drivers/BSP/${MCU_FAMILY}xx_${BOARD_NAME}
)

set(MCU_LINKER_SCRIPT ${ONTARGET_TEST_PATH}/${BOARD_NAME}/src/${MCU_PART}x_FLASH.ld)

set(USER_SOURCES "${ONTARGET_TEST_PATH}/${BOARD_NAME}/src/${MCU_FAMILY_LC}xx_hal_msp.c"
                 "${ONTARGET_TEST_PATH}/${BOARD_NAME}/src/${MCU_FAMILY_LC}xx_it.c"
                 "${ONTARGET_TEST_PATH}/${BOARD_NAME}/src/system_${MCU_FAMILY_LC}xx.c"
                 "${ONTARGET_TEST_PATH}/${BOARD_NAME}/src/main.c"
                 "${ONTARGET_TEST_PATH}/${BOARD_NAME}/src/posix.c")

set(BSP_SOURCES "${ONTARGET_TEST_PATH}/${BOARD_NAME}/Drivers/BSP/${MCU_FAMILY}xx_${BOARD_NAME}/${MCU_FAMILY_LC}xx_${BOARD_NAME}.c")

set(CMSIS_STARTUP "${ONTARGET_TEST_PATH}/${BOARD_NAME}/src/startup_${MCU_LINE}xx.s")

set(HAL_SOURCES "${ONTARGET_TEST_PATH}/${BOARD_NAME}/Drivers/${MCU_FAMILY}xx_HAL_Driver/Src/${MCU_FAMILY_LC}xx_hal.c"
                "${ONTARGET_TEST_PATH}/${BOARD_NAME}/Drivers/${MCU_FAMILY}xx_HAL_Driver/Src/${MCU_FAMILY_LC}xx_hal_cortex.c"
                "${ONTARGET_TEST_PATH}/${BOARD_NAME}/Drivers/${MCU_FAMILY}xx_HAL_Driver/Src/${MCU_FAMILY_LC}xx_hal_pwr_ex.c"
                "${ONTARGET_TEST_PATH}/${BOARD_NAME}/Drivers/${MCU_FAMILY}xx_HAL_Driver/Src/${MCU_FAMILY_LC}xx_hal_dma.c"
                "${ONTARGET_TEST_PATH}/${BOARD_NAME}/Drivers/${MCU_FAMILY}xx_HAL_Driver/Src/${MCU_FAMILY_LC}xx_hal_rcc.c"
                "${ONTARGET_TEST_PATH}/${BOARD_NAME}/Drivers/${MCU_FAMILY}xx_HAL_Driver/Src/${MCU_FAMILY_LC}xx_hal_rcc_ex.c"
                "${ONTARGET_TEST_PATH}/${BOARD_NAME}/Drivers/${MCU_FAMILY}xx_HAL_Driver/Src/${MCU_FAMILY_LC}xx_hal_uart.c"
                "${ONTARGET_TEST_PATH}/${BOARD_NAME}/Drivers/${MCU_FAMILY}xx_HAL_Driver/Src/${MCU_FAMILY_LC}xx_hal_uart_ex.c"
                "${ONTARGET_TEST_PATH}/${BOARD_NAME}/Drivers/${MCU_FAMILY}xx_HAL_Driver/Src/${MCU_FAMILY_LC}xx_hal_gpio.c")

set(SOURCE_FILES ${USER_SOURCES} ${HAL_SOURCES} ${CMSIS_STARTUP} ${BSP_SOURCES})

add_definitions(-D${MCU_LINE}xx
                -DUSE_HAL_LIBRARY
)

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
