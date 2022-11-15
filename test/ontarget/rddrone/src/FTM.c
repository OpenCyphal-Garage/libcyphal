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

#include "device_registers.h"	/* include peripheral declarations */
#include "FTM.h"

uint16_t CurrentCaptureVal = 0;
uint16_t PriorCaptureVal = 0;
uint16_t DeltaCapture = 0;

void FTM0_init(void)
{
	/**
	 * FTM0 Clocking:
	 * ==================================================
	 */
	PCC->PCCn[PCC_FTM0_INDEX] &= ~PCC_PCCn_CGC_MASK; 	/* Ensure clk disabled for config 	*/
	PCC->PCCn[PCC_FTM0_INDEX] |= PCC_PCCn_PCS(1 << 1)	/* Clock Src=1, 8 MHz SIRCDIV1_CLK */
                              |  PCC_PCCn_CGC_MASK;  	/* Enable clock for FTM regs 		*/

	/*!
	 * FTM0 Initialization:
	 * ===================================================
	 */
	FTM0->MODE |= FTM_MODE_WPDIS_MASK;  /* Write protect to registers disabled (default) 				*/
	FTM0->SC	=	FTM_SC_PWMEN0_MASK	/* Enable PWM channel 0 output									*/
					|FTM_SC_PWMEN1_MASK /* Enable PWM channel 1 output									*/
					|FTM_SC_PS(7);     	/* TOIE (Timer Overflow Interrupt Ena) = 0 (default) 			*/
										/* CPWMS (Center aligned PWM Select) = 0 (default, up count) 	*/
										/* CLKS (Clock source) = 0 (default, no clock; FTM disabled) 	*/
										/* PS (Prescaler factor) = 7. Prescaler = 128 					*/

	FTM0->COMBINE = 0x00000000;	/* FTM mode settings used: DECAPENx, MCOMBINEx, COMBINEx=0 */
	FTM0->POL = 0x00000000;    	/* Polarity for all channels is active high (default) 		*/
	FTM0->MOD = 62500 -1 ;     	/* FTM1 counter final value (used for PWM mode) 			*/
								/* FTM1 Period = MOD-CNTIN+0x0001 ~= 62500 ctr clks  		*/
								/* 8MHz /128 = 62.5kHz ->  ticks -> 1Hz 					*/
}

void FTM0_CH0_OC_init(void)
{
	/**
	 * FTM0, Channel 0 in Output Compare Mode:
	 * =================================================
	 */
	FTM0->CONTROLS[0].CnSC = FTM_CnSC_MSA_MASK
							|FTM_CnSC_ELSA_MASK;/* FTM0 ch0: Output Compare, toggle output on match 	*/
												/* CHIE (Chan Interrupt Ena)= 0 (default) 				*/
												/* MSB:MSA (chan Mode Select)= 0b01, Output Compare 	*/
												/* ELSB:ELSA (chan Edge or Level Select)= 0b01, toggle	*/

	FTM0->CONTROLS[0].CnV= 6250;         /* FTM0 ch 0 Compare Value= 6250  clks, 100ms toggle	*/
	FTM0->POL &= ~FTM_POL_POL0_MASK;     /* FTM0 ch 0 polarity = 0 (Default, active high) 		*/
}

void FTM0_CH1_PWM_init(void)
{
	/**
	 * FTM0, Channel 1 in PWM Mode:
	 * ==================================================
	 */
	FTM0->CONTROLS[1].CnSC = FTM_CnSC_MSB_MASK
							|FTM_CnSC_ELSB_MASK;  	/* FTM0 ch1: edge-aligned PWM, low true pulses 		*/
													/* CHIE (Chan Interrupt Ena) = 0 (default) 			*/
													/* MSB:MSA (chan Mode Select)=0b10, Edge Align PWM		*/
													/* ELSB:ELSA (chan Edge/Level Select)=0b10, low true 	*/

	FTM0->CONTROLS[1].CnV =  46875;	/* FTM0 ch1 compare value (~75% duty cycle) */
}

void FTM0_CH6_IC_init(void)
{
	/**
	 * FTM0, Channel 6 in Input Capture Mode:
	 * =================================================
	 */
	FTM0->CONTROLS[6].CnSC = FTM_CnSC_ELSB_MASK
							|FTM_CnSC_ELSA_MASK;  	/* FTM0 ch6: Input Capture rising or falling edge 		*/
													/* CHIE (Chan Interrupt Ena) = 0 (default) 			*/
													/* MSB:MSA (chan Mode Select)=0b00, Input Capture 		*/
													/* ELSB:ELSA (ch Edge/Level Select)=0b11, rise or fall	*/
}

void FTM0_CH0_output_compare(void)
{
	if (1==((FTM0->CONTROLS[0].CnSC & FTM_CnSC_CHF_MASK)>>FTM_CnSC_CHF_SHIFT))
	{
														/* - If chan flag is set: 				*/
		FTM0->CONTROLS[0].CnSC &= ~FTM_CnSC_CHF_MASK;  	/* Clear flag: read reg then set CHF=0 */
		if(  FTM0->CONTROLS[0].CnV==56250)
		{
										/* - If count at last value before end: 	*/
			FTM0->CONTROLS[0].CnV= 0 ;  /* Update compare value: to 0				*/
		}
		else
		{
			FTM0->CONTROLS[0].CnV= FTM0->CONTROLS[0].CnV + 6250 ;
			/* - else: */
			/* Update compare value: add 6250 to current value	*/
		}
	}
}

void FTM0_CH6_input_capture(void)
{
	if (1==((FTM0->CONTROLS[6].CnSC & FTM_CnSC_CHF_MASK)>>FTM_CnSC_CHF_SHIFT))
	{
														/* - If chan flag is set 				*/
		FTM0->CONTROLS[6].CnSC &= ~FTM_CnSC_CHF_MASK;   /* Clear flag: read reg then set CHF=0 */
		PriorCaptureVal = CurrentCaptureVal;            /* Record value of prior capture 		*/
		CurrentCaptureVal = FTM0->CONTROLS[6].CnV;      /* Record value of current capture 	*/

		DeltaCapture = CurrentCaptureVal - PriorCaptureVal;
		/* Will be 6250 clocks (100 msec) if connected to FTM0 CH0 */
	}
}

void start_FTM0_counter (void)
{
	FTM0->SC |= FTM_SC_CLKS(3);
	/* Start FTM0 counter with clk source = external clock (SOSCDIV1_CLK)*/
}

uint64_t libuavcan_media_s32k_get_monotonic_time_micros_isr_safe()
{
	// TODO: implement using four, chained channels of the FTM.
	return 0;
}
