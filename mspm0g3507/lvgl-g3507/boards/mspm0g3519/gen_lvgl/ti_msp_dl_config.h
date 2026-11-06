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
 *  DO NOT EDIT - This file is generated for the MSPM0G351X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G351X
#define CONFIG_MSPM0G3519

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



#define CPUCLK_FREQ                                                     32000000




/* Defines for TP_I2C */
#define TP_I2C_INST                                                         I2C2
#define TP_I2C_INST_IRQHandler                                   I2C2_IRQHandler
#define TP_I2C_INST_INT_IRQN                                       I2C2_INT_IRQn
#define TP_I2C_BUS_SPEED_HZ                                               100000
#define GPIO_TP_I2C_SDA_PORT                                               GPIOC
#define GPIO_TP_I2C_SDA_PIN                                        DL_GPIO_PIN_3
#define GPIO_TP_I2C_IOMUX_SDA                                    (IOMUX_PINCM77)
#define GPIO_TP_I2C_IOMUX_SDA_FUNC                     IOMUX_PINCM77_PF_I2C2_SDA
#define GPIO_TP_I2C_SCL_PORT                                               GPIOC
#define GPIO_TP_I2C_SCL_PIN                                        DL_GPIO_PIN_2
#define GPIO_TP_I2C_IOMUX_SCL                                    (IOMUX_PINCM76)
#define GPIO_TP_I2C_IOMUX_SCL_FUNC                     IOMUX_PINCM76_PF_I2C2_SCL


/* Defines for DBG_UART */
#define DBG_UART_INST                                                      UART0
#define DBG_UART_INST_FREQUENCY                                         32000000
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
#define DBG_UART_IBRD_32_MHZ_115200_BAUD                                    (17)
#define DBG_UART_FBRD_32_MHZ_115200_BAUD                                    (23)




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
#define GPIO_LCD_SPI_SCLK_PIN                                    DL_GPIO_PIN_31
#define GPIO_LCD_SPI_IOMUX_SCLK                                 (IOMUX_PINCM68)
#define GPIO_LCD_SPI_IOMUX_SCLK_FUNC                 IOMUX_PINCM68_PF_SPI1_SCLK



/* Port definition for Pin Group PB */
#define PB_PORT                                                          (GPIOB)

/* Defines for LCD_DC: GPIOB.9 with pinCMx 26 on package pin 43 */
#define PB_LCD_DC_PIN                                            (DL_GPIO_PIN_9)
#define PB_LCD_DC_IOMUX                                          (IOMUX_PINCM26)
/* Defines for LCD_CS: GPIOB.12 with pinCMx 29 on package pin 46 */
#define PB_LCD_CS_PIN                                           (DL_GPIO_PIN_12)
#define PB_LCD_CS_IOMUX                                          (IOMUX_PINCM29)
/* Defines for TP_RST: GPIOB.11 with pinCMx 28 on package pin 45 */
#define PB_TP_RST_PIN                                           (DL_GPIO_PIN_11)
#define PB_TP_RST_IOMUX                                          (IOMUX_PINCM28)
/* Defines for TP_INT: GPIOB.14 with pinCMx 31 on package pin 48 */
#define PB_TP_INT_PIN                                           (DL_GPIO_PIN_14)
#define PB_TP_INT_IOMUX                                          (IOMUX_PINCM31)
/* Port definition for Pin Group PA */
#define PA_PORT                                                          (GPIOA)

/* Defines for LED1: GPIOA.0 with pinCMx 1 on package pin 1 */
#define PA_LED1_PIN                                              (DL_GPIO_PIN_0)
#define PA_LED1_IOMUX                                             (IOMUX_PINCM1)
/* Port definition for Pin Group PC */
#define PC_PORT                                                          (GPIOC)

/* Defines for LCD_BL: GPIOC.4 with pinCMx 78 on package pin 67 */
#define PC_LCD_BL_PIN                                            (DL_GPIO_PIN_4)
#define PC_LCD_BL_IOMUX                                          (IOMUX_PINCM78)
/* Defines for LCD_RST: GPIOC.5 with pinCMx 79 on package pin 68 */
#define PC_LCD_RST_PIN                                           (DL_GPIO_PIN_5)
#define PC_LCD_RST_IOMUX                                         (IOMUX_PINCM79)


/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_TP_I2C_init(void);
void SYSCFG_DL_DBG_UART_init(void);
void SYSCFG_DL_LCD_SPI_init(void);


bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
