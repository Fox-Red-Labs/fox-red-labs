/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *  ======== ti_drivers_config.h ========
 *  Configured TI-Drivers module declarations
 *
 *  The macros defines herein are intended for use by applications which
 *  directly include this header. These macros should NOT be hard coded or
 *  copied into library source code.
 *
 *  Symbols declared as const are intended for use with libraries.
 *  Library source code must extern the correct symbol--which is resolved
 *  when the application is linked.
 *
 */

/*
 * Modified by Fox Red Labs, 2026. Derived from TI's drivers/adc_singlechannel and drivers/uart_echo ti_drivers_config (MSPM0 SDK 2.11.00.07).
 * The TI copyright and license above are kept as in the original file.
 */
/* Three ways, TI Drivers board configuration for the LP-MSPM0L1306.
 * MSPM0 TI Drivers have no SysConfig module: TI's examples ship this file as hand-written C.
 * Cut down from TI's adc_singlechannel and uart_echo examples (MSPM0 SDK 2.11.00.07). */
#ifndef ti_drivers_config_h
#define ti_drivers_config_h

#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>

#define CONFIG_ADC_0 0
#define CONFIG_UART_0 0
#define CONFIG_UART_COUNT 1
#define CONFIG_GPIO_LED_0 0                        /* PA0, LED1 (open drain, J2 and J10 2:3) */
#define CONFIG_GPIO_LED_0_IOMUX (IOMUX_PINCM1)
#define CONFIG_GPIO_ADC_0_AIN 15                   /* PA15, ADC0 channel 9, the onboard thermistor */

#endif
