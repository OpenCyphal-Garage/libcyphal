/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
/*
 * Copyright (c) 2014 - 2016, Freescale Semiconductor, Inc.
 * Copyright (c) 2016 - 2018, NXP.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY NXP "AS IS" AND ANY EXPRESSED OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL NXP OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <errno.h>
#include <iostream>

#include "device_registers.h"  // include peripheral declarations S32K144
#include "clocks_and_modes.h"
#include "LPUART.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

extern "C"
{
    char data = 0;
    void PORT_init(void)
    {
        /*!
         * Pins definitions
         * ===================================================
         *
         * Pin number        | Function
         * ----------------- |------------------
         * PTC6              | UART1 TX
         * PTC7              | UART1 RX
         */
        PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK; /* Enable clock for PORTC        */
        PORTC->PCR[6] |= PORT_PCR_MUX(2);                /* Port C6: MUX = ALT2, UART1 TX */
        PORTC->PCR[7] |= PORT_PCR_MUX(2);                /* Port C7: MUX = ALT2, UART1 RX */
    }

    void WDOG_disable(void)
    {
        WDOG->CNT   = 0xD928C520; /* Unlock watchdog         */
        WDOG->TOVAL = 0x0000FFFF; /* Maximum timeout value   */
        WDOG->CS    = 0x00002100; /* Disable watchdog        */
    }

    // TODO: set led/gpio on these faults.
    __attribute__((naked)) void HardFault_Handler(void)
    {
        for (;;)
        {
        }
    }

    __attribute__((naked)) void MemManage_Handler(void)
    {
        for (;;)
        {
        }
    }

    __attribute__((naked)) void BusFault_Handler(void)
    {
        for (;;)
        {
        }
    }

    __attribute__((naked)) void UsageFault_Handler(void)
    {
        for (;;)
        {
        }
    }

    // TODO reenable watchdog. Set LED/gpio on wdt timeout.
    __attribute__((naked)) void WDOG_EWM_IRQHandler(void)
    {
        for (;;)
        {
        }
    }
}  // extern "C"

int main(void)
{
    /*!
     * Initialization:
     * =======================
     */
    WDOG_disable();        /* Disable WDOG */
    SOSC_init_8MHz();      /* Initialize system oscilator for 8 MHz xtal */
    SPLL_init_160MHz();    /* Initialize SPLL to 160 MHz with 8 MHz SOSC */
    NormalRUNmode_80MHz(); /* Init clocks: 80 MHz sysclk & core, 40 MHz bus, 20 MHz flash */
    PORT_init();           /* Configure ports */

    LPUART1_init();                                            /* Initialize LPUART @ 9600*/
    LPUART1_transmit_string("Running LPUART example\n\r");     /* Transmit char string */
    LPUART1_transmit_string("Input character to echo...\n\r"); /* Transmit char string */

    char program_name[] = "libuavcan_ontarget";

    // the arguments array
    char* argv[] = {program_name};

    int argc = 1;

    // This also initialized googletest.
    testing::InitGoogleMock(&argc, argv);

    int result = RUN_ALL_TESTS();
    (void) result;
    /*!
     * Infinite for:
     * ========================
     */
    for (;;)
    {
        LPUART1_transmit_char(result);
        LPUART1_receive_and_echo_char();  // Wait for input char, receive & echo it
    }
}
