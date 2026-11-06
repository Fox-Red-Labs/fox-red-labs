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
 */

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G350X
#define CONFIG_MSPM0G3507

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)



#define CPUCLK_FREQ                                                     80000000
/* Defines for SYSPLL_ERR_01 Workaround */
/* Represent 1.000 as 1000 */
#define FLOAT_TO_INT_SCALE                                               (1000U)
#define FCC_EXPECTED_RATIO                                                  2500
#define FCC_UPPER_BOUND                       (FCC_EXPECTED_RATIO * (1 + 0.003))
#define FCC_LOWER_BOUND                       (FCC_EXPECTED_RATIO * (1 - 0.003))

bool SYSCFG_DL_SYSCTL_SYSPLL_init(void);



/* Defines for TP_I2C */
#define TP_I2C_INST                                                         I2C1
#define TP_I2C_INST_IRQHandler                                   I2C1_IRQHandler
#define TP_I2C_INST_INT_IRQN                                       I2C1_INT_IRQn
#define TP_I2C_BUS_SPEED_HZ                                               100000
#define GPIO_TP_I2C_SDA_PORT                                               GPIOB
#define GPIO_TP_I2C_SDA_PIN                                        DL_GPIO_PIN_3
#define GPIO_TP_I2C_IOMUX_SDA                                    (IOMUX_PINCM16)
#define GPIO_TP_I2C_IOMUX_SDA_FUNC                     IOMUX_PINCM16_PF_I2C1_SDA
#define GPIO_TP_I2C_SCL_PORT                                               GPIOB
#define GPIO_TP_I2C_SCL_PIN                                        DL_GPIO_PIN_2
#define GPIO_TP_I2C_IOMUX_SCL                                    (IOMUX_PINCM15)
#define GPIO_TP_I2C_IOMUX_SCL_FUNC                     IOMUX_PINCM15_PF_I2C1_SCL


/* Defines for DBG_UART */
#define DBG_UART_INST                                                      UART0
#define DBG_UART_INST_FREQUENCY                                         40000000
#define DBG_UART_INST_IRQHandler                                UART0_IRQHandler
#define DBG_UART_INST_INT_IRQN                                    UART0_INT_IRQn
#define GPIO_DBG_UART_RX_PORT                                              GPIOA
#define GPIO_DBG_UART_TX_PORT                                              GPIOA
#define GPIO_DBG_UART_RX_PIN                                      DL_GPIO_PIN_11
#define GPIO_DBG_UART_TX_PIN                                      DL_GPIO_PIN_10
#define GPIO_DBG_UART_IOMUX_RX                                   (IOMUX_PINCM22)
#define GPIO_DBG_UART_IOMUX_TX                                   (IOMUX_PINCM21)
#define GPIO_DBG_UART_IOMUX_RX_FUNC                    IOMUX_PINCM22_PF_UART0_RX
#define GPIO_DBG_UART_IOMUX_TX_FUNC                    IOMUX_PINCM21_PF_UART0_TX
#define DBG_UART_BAUD_RATE                                              (115200)
#define DBG_UART_IBRD_40_MHZ_115200_BAUD                                    (21)
#define DBG_UART_FBRD_40_MHZ_115200_BAUD                                    (45)




/* Defines for LCD_SPI */
#define LCD_SPI_INST                                                       SPI1
#define LCD_SPI_INST_IRQHandler                                 SPI1_IRQHandler
#define LCD_SPI_INST_INT_IRQN                                     SPI1_INT_IRQn
#define GPIO_LCD_SPI_PICO_PORT                                            GPIOB
#define GPIO_LCD_SPI_PICO_PIN                                     DL_GPIO_PIN_8
#define GPIO_LCD_SPI_IOMUX_PICO                                 (IOMUX_PINCM25)
#define GPIO_LCD_SPI_IOMUX_PICO_FUNC                 IOMUX_PINCM25_PF_SPI1_PICO
#define GPIO_LCD_SPI_POCI_PORT                                            GPIOB
#define GPIO_LCD_SPI_POCI_PIN                                     DL_GPIO_PIN_7
#define GPIO_LCD_SPI_IOMUX_POCI                                 (IOMUX_PINCM24)
#define GPIO_LCD_SPI_IOMUX_POCI_FUNC                 IOMUX_PINCM24_PF_SPI1_POCI
/* GPIO configuration for LCD_SPI */
#define GPIO_LCD_SPI_SCLK_PORT                                            GPIOB
#define GPIO_LCD_SPI_SCLK_PIN                                     DL_GPIO_PIN_9
#define GPIO_LCD_SPI_IOMUX_SCLK                                 (IOMUX_PINCM26)
#define GPIO_LCD_SPI_IOMUX_SCLK_FUNC                 IOMUX_PINCM26_PF_SPI1_SCLK



/* Port definition for Pin Group PB */
#define PB_PORT                                                          (GPIOB)

/* Defines for LCD_DC: GPIOB.1 with pinCMx 13 on package pin 48 */
#define PB_LCD_DC_PIN                                            (DL_GPIO_PIN_1)
#define PB_LCD_DC_IOMUX                                          (IOMUX_PINCM13)
/* Defines for LCD_RST: GPIOB.4 with pinCMx 17 on package pin 52 */
#define PB_LCD_RST_PIN                                           (DL_GPIO_PIN_4)
#define PB_LCD_RST_IOMUX                                         (IOMUX_PINCM17)
/* Defines for LCD_CS: GPIOB.6 with pinCMx 23 on package pin 58 */
#define PB_LCD_CS_PIN                                            (DL_GPIO_PIN_6)
#define PB_LCD_CS_IOMUX                                          (IOMUX_PINCM23)
/* Defines for TP_RST: GPIOB.0 with pinCMx 12 on package pin 47 */
#define PB_TP_RST_PIN                                            (DL_GPIO_PIN_0)
#define PB_TP_RST_IOMUX                                          (IOMUX_PINCM12)
/* Defines for TP_INT: GPIOB.16 with pinCMx 33 on package pin 4 */
#define PB_TP_INT_PIN                                           (DL_GPIO_PIN_16)
#define PB_TP_INT_IOMUX                                          (IOMUX_PINCM33)
/* Port definition for Pin Group PA */
#define PA_PORT                                                          (GPIOA)

/* Defines for LCD_BL: GPIOA.28 with pinCMx 3 on package pin 35 */
#define PA_LCD_BL_PIN                                           (DL_GPIO_PIN_28)
#define PA_LCD_BL_IOMUX                                           (IOMUX_PINCM3)
/* Defines for LED1: GPIOA.0 with pinCMx 1 on package pin 33 */
#define PA_LED1_PIN                                              (DL_GPIO_PIN_0)
#define PA_LED1_IOMUX                                             (IOMUX_PINCM1)


/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_SYSCTL_CLK_init(void);

bool SYSCFG_DL_SYSCTL_SYSPLL_init(void);
void SYSCFG_DL_TP_I2C_init(void);
void SYSCFG_DL_DBG_UART_init(void);
void SYSCFG_DL_LCD_SPI_init(void);


bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
