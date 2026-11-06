/* Force-included when F3 SDK (CC23xx/CC27xx) TI Drivers app code is built with the MSPM0 SDK.
 * Every line here is a gap between two TI SDKs that share the TI Drivers API. Keep one gap per line. */
#pragma once
/* SPI: F3 has 4-wire Motorola frames only, named without "MOTO4". */
#define SPI_POL0_PHA0 SPI_MOTO4_POL0_PHA0
#define SPI_POL0_PHA1 SPI_MOTO4_POL0_PHA1
#define SPI_POL1_PHA0 SPI_MOTO4_POL1_PHA0
#define SPI_POL1_PHA1 SPI_MOTO4_POL1_PHA1
/* UART: same 13 calls, named UART (not UART2) on MSPM0. */
#include <ti/drivers/UART.h>
#define UART2_Handle UART_Handle
#define UART2_Params UART_Params
#define UART2_Params_init UART_Params_init
#define UART2_open UART_open
#define UART2_close UART_close
#define UART2_read UART_read
#define UART2_write UART_write
#define UART2_readTimeout UART_readTimeout
#define UART2_writeTimeout UART_writeTimeout
#define UART2_readCancel UART_readCancel
#define UART2_writeCancel UART_writeCancel
#define UART2_flushRx UART_flushRx
#define UART2_rxEnable UART_rxEnable
#define UART2_rxDisable UART_rxDisable
#define UART2_getRxCount UART_getRxCount
#define UART2_STATUS_SUCCESS UART_STATUS_SUCCESS
#define UART2_Mode_BLOCKING UART_Mode_BLOCKING
#define UART2_Mode_CALLBACK UART_Mode_CALLBACK
#define UART2_Mode_NONBLOCKING UART_Mode_NONBLOCKING
/* UART board name: F3 SysConfig names the port CONFIG_UART2_0; the MSPM0 board file calls it CONFIG_UART_0. */
#define CONFIG_UART2_0 CONFIG_UART_0
/* I2C target board name: F3 SysConfig names the instance CONFIG_I2C_0; the MSPM0 board file calls it CONFIG_I2C_TARGET_0. */
#define CONFIG_I2C_0 CONFIG_I2C_TARGET_0
/* LED level: the F3 examples write CONFIG_GPIO_LED_ON; the LaunchPad LEDs are active low on MSPM0 (same spelling as its board file). */
#define CONFIG_GPIO_LED_ON (0)
#define CONFIG_GPIO_LED_OFF (1)
/* LED 1: the MSPM0 LaunchPad board files configure one LED, so F3's second LED shares it. */
#define CONFIG_GPIO_LED_1 CONFIG_GPIO_LED_0
#ifdef FR_NO_GPIO_DRIVER
/* GPIO: this MSPM0 example's board file has no TI Drivers GPIO table (it drives the LED with DriverLib on GPIOA pin 0,
 * already set up by SYSCFG_DL_init), so the LED calls of the F3 app become the same DriverLib calls. LED 0 only. */
#include <ti/drivers/GPIO.h>
#define CONFIG_GPIO_LED_0 0
#define GPIO_init() ((void) 0)
#define GPIO_setConfig(index, config) ((void) (index), (void) (config), 0)
#define GPIO_write(index, value) ((value) ? DL_GPIO_setPins(GPIO_GRP_0_PORT, GPIO_GRP_0_PIN_0_PIN) : DL_GPIO_clearPins(GPIO_GRP_0_PORT, GPIO_GRP_0_PIN_0_PIN))
#define GPIO_toggle(index) DL_GPIO_togglePins(GPIO_GRP_0_PORT, GPIO_GRP_0_PIN_0_PIN)
#endif
