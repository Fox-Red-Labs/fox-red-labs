/*
 * Copyright (c) 2023, Texas Instruments Incorporated
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
 */

/*
 * Modified by Fox Red Labs, 2026. Derived from TI's drivers/adc_singlechannel and drivers/uart_echo ti_drivers_config (MSPM0 SDK 2.11.00.07).
 * The TI copyright and license above are kept as in the original file.
 */
/* Three ways, TI Drivers board configuration for the LP-MSPM0G3507 (hand-written C, see the .h). */
#include "ti_drivers_config.h"
#include <ti/drivers/GPIO.h>
#include <ti/drivers/gpio/GPIOMSPM0.h>
#include <ti/drivers/ADC.h>
#include <ti/drivers/adc/ADCMSPM0.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/uart/UARTMSPM0.h>

/* GPIO: 60 pins, PA0 to PB27. Every entry is GPIO_DO_NOT_CONFIG, as TI's examples do for their LED:
 * GPIO_init takes the pin-mux index from each entry's low byte, so an entry without one would set up PINCM1 (PA0).
 * The glue sets the LED with its mux index; ADC_open sets the ADC pin. */
const uint_least8_t GPIO_pinLowerBound = 0;
const uint_least8_t GPIO_pinUpperBound = 60;
GPIO_PinConfig gpioPinConfigs[60] = {[0 ... 59] = GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG};
GPIO_CallbackFxn gpioCallbackFunctions[60];
void *gpioUserArgs[60];
const GPIO_Config GPIO_config = {.configs = gpioPinConfigs,
    .callbacks   = gpioCallbackFunctions,
    .userArgs    = gpioUserArgs,
    .intPriority = (~0)};

/* ADC: ADC0 channel 2 on PA25, internal 2.5 V reference (powered in ti_msp_dl_config.c), SYSOSC / 8,
 * 500-clock sample window (125 us), the same settings as the DriverLib and Zephyr ways. */
ADCMSPM0_Object adcObjects[1];
const ADCMSPM0_HWAttrs adcHWAttrs[1] = {
    {.adc                     = ADC0,
        .adcInputDIO          = CONFIG_GPIO_ADC_0_AIN,
        .adcInputPincm        = IOMUX_PINCM55,
        .adcInputPinMux       = IOMUX_PINCM55_PF_GPIOA_DIO25,
        .adcPosRefDIO         = GPIO_INVALID_INDEX,
        .adcNegRefDIO         = GPIO_INVALID_INDEX,
        .adcChannel           = 2,
        .refSource            = ADCMSPM0_INTERNAL_REFERENCE,
        .samplingDuration     = 500,
        .refVoltage           = 2500000, /* uV */
        .resolutionBits       = ADCMSPM0_RESOLUTION_12_BIT,
        .adcClkkDivider       = ADCMSPM0_CLKDIV_8,
        .adcClkSelect         = ADCMSPM0_CLK_SYSOSC,
        .adcClkFreqRange      = ADCMSPM0_CLK_FREQ_RANGE_24TO32,
        .conversionMode       = ADCMSPM0_SINGLE_CH_SINGLE_CONV,
        .conversionStartAddr  = 0,
        .conversionEndAddr    = 0,
        .repeatConversionMode = ADCMSPM0_REPEAT_MODE_ENABLED,
        .samplingMode         = ADCMSPM0_SAMPLING_MODE_AUTO,
        .sampleTrigger        = ADCMSPM0_SAMPLING_TRIG_SW,
        .conversionDataFormat = ADCMSPM0_CONV_DATA_FORMAT_UNSIGNED,
        .sampleTimerSrc       = ADCMSPM0_SAMP_TMR_SOURCE_SCOMP0,
        .conversionTrigger    = ADCMSPM0_NEXT_CONV_WITH_TRIG,
        .adcHWAveraging       = ADCMSPM0_HW_AVG_DISABLED,
        .idxMEMCTLx           = 0},
};
const ADC_Config ADC_config[1] = {
    {.fxnTablePtr = &ADCMSPM0_fxnTable, .object = &adcObjects[0], .hwAttrs = &adcHWAttrs[0]},
};
const uint_least8_t ADC_count = 1;

/* UART: UART0 on PA10 (TX) and PA11 (RX), the XDS110 backchannel. Baud rate comes from UART_Params. */
const uint_least8_t UART_count = CONFIG_UART_COUNT;
static uint8_t rxBuffer[32];
static uint8_t txBuffer[32];
static const UARTMSP_HWAttrs UARTMSPHWAttrs[1] = {
    {
        .regs          = UART0,
        .irq           = UART0_INT_IRQn,
        .rxPin         = IOMUX_PINCM22, /* PA11 */
        .rxPinFunction = IOMUX_PINCM22_PF_UART0_RX,
        .txPin         = IOMUX_PINCM21, /* PA10 */
        .txPinFunction = IOMUX_PINCM21_PF_UART0_TX,
        .mode          = DL_UART_MODE_NORMAL,
        .direction     = DL_UART_DIRECTION_TX_RX,
        .flowControl   = DL_UART_FLOW_CONTROL_NONE,
        .clockSource   = DL_UART_CLOCK_BUSCLK,
        .clockDivider  = DL_UART_CLOCK_DIVIDE_RATIO_4,
        .rxIntFifoThr  = DL_UART_RX_FIFO_LEVEL_ONE_ENTRY,
        .txIntFifoThr  = DL_UART_TX_FIFO_LEVEL_EMPTY,
    },
};
UART_Data_Object UARTObject[1] = {
    {
        .object = {.supportFxns = &UARTMSPSupportFxns, .buffersSupported = true,
            .eventsSupported = false, .callbacksSupported = false, .dmaSupported = false},
        .buffersObject = {.rxBufPtr = rxBuffer, .txBufPtr = txBuffer,
            .rxBufSize = sizeof(rxBuffer), .txBufSize = sizeof(txBuffer)},
    },
};
const UART_Config UART_config[1] = {
    {&UARTObject[0], &UARTMSPHWAttrs[0]},
};
void UART0_IRQHandler(void)
{
    UARTMSP_interruptHandler((UART_Handle) &UART_config[0]);
}
void UARTMSP_eventCallback(UART_Handle handle, uint32_t event, uint32_t data, void *userArg)
{
}
