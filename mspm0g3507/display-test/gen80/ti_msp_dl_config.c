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
 *  ============ ti_msp_dl_config.c =============
 *  Configured MSPM0 DriverLib module definitions
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */

#include "ti_msp_dl_config.h"

DL_SPI_backupConfig gLCD_SPIBackup;

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform any initialization needed before using any board APIs
 */
SYSCONFIG_WEAK void SYSCFG_DL_init(void)
{
    SYSCFG_DL_initPower();
    SYSCFG_DL_GPIO_init();
    /* Module-Specific Initializations*/
    SYSCFG_DL_SYSCTL_init();
    SYSCFG_DL_TP_I2C_init();
    SYSCFG_DL_LCD_SPI_init();
    SYSCFG_DL_SYSCTL_CLK_init();
    /* Ensure backup structures have no valid state */
	gLCD_SPIBackup.backupRdy 	= false;

}
/*
 * User should take care to save and restore register configuration in application.
 * See Retention Configuration section for more details.
 */
SYSCONFIG_WEAK bool SYSCFG_DL_saveConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_SPI_saveConfiguration(LCD_SPI_INST, &gLCD_SPIBackup);

    return retStatus;
}


SYSCONFIG_WEAK bool SYSCFG_DL_restoreConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_SPI_restoreConfiguration(LCD_SPI_INST, &gLCD_SPIBackup);

    return retStatus;
}

SYSCONFIG_WEAK void SYSCFG_DL_initPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_I2C_reset(TP_I2C_INST);
    DL_SPI_reset(LCD_SPI_INST);

    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_I2C_enablePower(TP_I2C_INST);
    DL_SPI_enablePower(LCD_SPI_INST);
    delay_cycles(POWER_STARTUP_DELAY);
}

SYSCONFIG_WEAK void SYSCFG_DL_GPIO_init(void)
{

    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_TP_I2C_IOMUX_SDA,
        GPIO_TP_I2C_IOMUX_SDA_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_TP_I2C_IOMUX_SCL,
        GPIO_TP_I2C_IOMUX_SCL_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableHiZ(GPIO_TP_I2C_IOMUX_SDA);
    DL_GPIO_enableHiZ(GPIO_TP_I2C_IOMUX_SCL);

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_LCD_SPI_IOMUX_SCLK, GPIO_LCD_SPI_IOMUX_SCLK_FUNC);
    DL_GPIO_initPeripheralOutputFunction(
        GPIO_LCD_SPI_IOMUX_PICO, GPIO_LCD_SPI_IOMUX_PICO_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_LCD_SPI_IOMUX_POCI, GPIO_LCD_SPI_IOMUX_POCI_FUNC);

    DL_GPIO_initDigitalOutput(PB_LCD_DC_IOMUX);

    DL_GPIO_initDigitalOutput(PB_LCD_RST_IOMUX);

    DL_GPIO_initDigitalOutput(PB_LCD_CS_IOMUX);

    DL_GPIO_initDigitalOutput(PB_TP_RST_IOMUX);

    DL_GPIO_initDigitalInputFeatures(PB_TP_INT_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalOutput(PA_LCD_BL_IOMUX);

    DL_GPIO_initDigitalOutput(PA_LED1_IOMUX);

    DL_GPIO_clearPins(PA_PORT, PA_LCD_BL_PIN |
		PA_LED1_PIN);
    DL_GPIO_enableOutput(PA_PORT, PA_LCD_BL_PIN |
		PA_LED1_PIN);
    DL_GPIO_clearPins(PB_PORT, PB_LCD_DC_PIN);
    DL_GPIO_setPins(PB_PORT, PB_LCD_RST_PIN |
		PB_LCD_CS_PIN |
		PB_TP_RST_PIN);
    DL_GPIO_enableOutput(PB_PORT, PB_LCD_DC_PIN |
		PB_LCD_RST_PIN |
		PB_LCD_CS_PIN |
		PB_TP_RST_PIN);

}


static const DL_SYSCTL_SYSPLLConfig gSYSPLLConfig = {
    .inputFreq              = DL_SYSCTL_SYSPLL_INPUT_FREQ_16_32_MHZ,
	.rDivClk2x              = 3,
	.rDivClk1               = 0,
	.rDivClk0               = 0,
	.enableCLK2x            = DL_SYSCTL_SYSPLL_CLK2X_ENABLE,
	.enableCLK1             = DL_SYSCTL_SYSPLL_CLK1_DISABLE,
	.enableCLK0             = DL_SYSCTL_SYSPLL_CLK0_DISABLE,
	.sysPLLMCLK             = DL_SYSCTL_SYSPLL_MCLK_CLK2X,
	.sysPLLRef              = DL_SYSCTL_SYSPLL_REF_SYSOSC,
	.qDiv                   = 9,
	.pDiv                   = DL_SYSCTL_SYSPLL_PDIV_2
};

SYSCONFIG_WEAK bool SYSCFG_DL_SYSCTL_SYSPLL_init(void)
{
    bool fFCCRatioStatus = false;
    uint32_t fFCCSysoscCount;
    uint32_t fFCCPllCount;
    uint32_t fFCCRatio;
    uint32_t fccTimeOutCounter;

    DL_SYSCTL_setFCCPeriods( DL_SYSCTL_FCC_TRIG_CNT_01 );

    /* Measuring PLL. */
    DL_SYSCTL_configFCC(DL_SYSCTL_FCC_TRIG_TYPE_RISE_RISE,
                        DL_SYSCTL_FCC_TRIG_SOURCE_LFCLK,
                        DL_SYSCTL_FCC_CLOCK_SOURCE_SYSPLLCLK2X);
    /* Get SYSPLL frequency using FCC */
    fccTimeOutCounter = 0;
    DL_SYSCTL_startFCC();
    while (DL_SYSCTL_isFCCDone() == 0) {
        delay_cycles(977);  /* 1x LFCLK cycle = 32MHz/32.768kHz = 977, 30.5us */
        fccTimeOutCounter++;
        if(fccTimeOutCounter > 65){
            /* Timeout set to approximately 2ms (user-customizable) */
            break;
        }
    }

    /* get measA= SYSPLLCLK2X freq wrt LFOSC*/
    fFCCPllCount = DL_SYSCTL_readFCC();

    /* Measuring SYSPLL Source */
    DL_SYSCTL_configFCC(DL_SYSCTL_FCC_TRIG_TYPE_RISE_RISE,
                        DL_SYSCTL_FCC_TRIG_SOURCE_LFCLK,
                        DL_SYSCTL_FCC_CLOCK_SOURCE_SYSOSC);
    /* Get SYSPLL frequency using FCC */
    fccTimeOutCounter = 0;
    DL_SYSCTL_startFCC();
    while (DL_SYSCTL_isFCCDone() == 0) {
        delay_cycles(977);  /* 1x LFCLK cycle = 32MHz/32.768kHz = 977, 30.5us */
        fccTimeOutCounter++;
        if(fccTimeOutCounter > 65){
            /* Timeout set to approximately 2ms (user-customizable) */
            break;
        }
    }

    /* get measB= SYSOSC freq wrt LFOSC*/
    fFCCSysoscCount = DL_SYSCTL_readFCC();

    /* Get ratio of both measurements*/
    fFCCRatio = (fFCCPllCount * FLOAT_TO_INT_SCALE) / fFCCSysoscCount;
    /* Check ratio is within bounds*/
    if ((FCC_LOWER_BOUND <  fFCCRatio) && (fFCCRatio < FCC_UPPER_BOUND))
    {
        /* ratio is good for proceeding into application code. */
        fFCCRatioStatus = true;
    }

    return fFCCRatioStatus;
}
SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_init(void)
{

	//Low Power Mode is configured to be SLEEP0
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);
    DL_SYSCTL_setFlashWaitState(DL_SYSCTL_FLASH_WAIT_STATE_2);

    
	DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
    DL_SYSCTL_configSYSPLL((DL_SYSCTL_SYSPLLConfig *) &gSYSPLLConfig);

    /*
     * [SYSPLL_ERR_01]
     * PLL Incorrect locking WA start.
     * Insert after every PLL enable.
     * This can lead an infinite loop if the condition persists
     * and can block entry to the application code.
     */

    while (SYSCFG_DL_SYSCTL_SYSPLL_init() == false)
    {
        /* Toggle SYSPLL enable to re-enable SYSPLL and re-check incorrect locking */
        DL_SYSCTL_disableSYSPLL();
        DL_SYSCTL_enableSYSPLL();

        /* Wait until SYSPLL startup is stabilized*/
        while ((DL_SYSCTL_getClockStatus() & SYSCTL_CLKSTATUS_SYSPLLGOOD_MASK) != DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD){}
    }
    DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_2);
    DL_SYSCTL_setMCLKSource(SYSOSC, HSCLK, DL_SYSCTL_HSCLK_SOURCE_SYSPLL);

}
SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_CLK_init(void) {
    while ((DL_SYSCTL_getClockStatus() & (DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD
		 | DL_SYSCTL_CLK_STATUS_HSCLK_GOOD
		 | DL_SYSCTL_CLK_STATUS_LFOSC_GOOD))
	       != (DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD
		 | DL_SYSCTL_CLK_STATUS_HSCLK_GOOD
		 | DL_SYSCTL_CLK_STATUS_LFOSC_GOOD))
	{
		/* Ensure that clocks are in default POR configuration before initialization.
		* Additionally once LFXT is enabled, the internal LFOSC is disabled, and cannot
		* be re-enabled other than by executing a BOOTRST. */
		;
	}
}



static const DL_I2C_ClockConfig gTP_I2CClockConfig = {
    .clockSel = DL_I2C_CLOCK_BUSCLK,
    .divideRatio = DL_I2C_CLOCK_DIVIDE_1,
};

SYSCONFIG_WEAK void SYSCFG_DL_TP_I2C_init(void) {

    DL_I2C_setClockConfig(TP_I2C_INST,
        (DL_I2C_ClockConfig *) &gTP_I2CClockConfig);
    DL_I2C_setAnalogGlitchFilterPulseWidth(TP_I2C_INST,
        DL_I2C_ANALOG_GLITCH_FILTER_WIDTH_50NS);
    DL_I2C_enableAnalogGlitchFilter(TP_I2C_INST);

    /* Configure Controller Mode */
    DL_I2C_resetControllerTransfer(TP_I2C_INST);
    /* Set frequency to 100000 Hz*/
    DL_I2C_setTimerPeriod(TP_I2C_INST, 39);
    DL_I2C_setControllerTXFIFOThreshold(TP_I2C_INST, DL_I2C_TX_FIFO_LEVEL_EMPTY);
    DL_I2C_setControllerRXFIFOThreshold(TP_I2C_INST, DL_I2C_RX_FIFO_LEVEL_BYTES_1);
    DL_I2C_enableControllerClockStretching(TP_I2C_INST);


    /* Enable module */
    DL_I2C_enableController(TP_I2C_INST);


}

static const DL_SPI_Config gLCD_SPI_config = {
    .mode        = DL_SPI_MODE_CONTROLLER,
    .frameFormat = DL_SPI_FRAME_FORMAT_MOTO3_POL0_PHA0,
    .parity      = DL_SPI_PARITY_NONE,
    .dataSize    = DL_SPI_DATA_SIZE_8,
    .bitOrder    = DL_SPI_BIT_ORDER_MSB_FIRST,
};

static const DL_SPI_ClockConfig gLCD_SPI_clockConfig = {
    .clockSel    = DL_SPI_CLOCK_BUSCLK,
    .divideRatio = DL_SPI_CLOCK_DIVIDE_RATIO_1
};

SYSCONFIG_WEAK void SYSCFG_DL_LCD_SPI_init(void) {
    DL_SPI_setClockConfig(LCD_SPI_INST, (DL_SPI_ClockConfig *) &gLCD_SPI_clockConfig);

    DL_SPI_init(LCD_SPI_INST, (DL_SPI_Config *) &gLCD_SPI_config);

    /* Configure Controller mode */
    /*
     * Set the bit rate clock divider to generate the serial output clock
     *     outputBitRate = (spiInputClock) / ((1 + SCR) * 2)
     *     8000000 = (80000000)/((1 + 4) * 2)
     */
    DL_SPI_setBitRateSerialClockDivider(LCD_SPI_INST, 4);
    /* Set RX and TX FIFO threshold levels */
    DL_SPI_setFIFOThreshold(LCD_SPI_INST, DL_SPI_RX_FIFO_LEVEL_1_2_FULL, DL_SPI_TX_FIFO_LEVEL_1_2_EMPTY);

    /* Enable module */
    DL_SPI_enable(LCD_SPI_INST);
}

