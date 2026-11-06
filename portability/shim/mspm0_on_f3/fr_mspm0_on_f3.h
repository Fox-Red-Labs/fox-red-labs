/* Force-included when MSPM0 SDK TI Drivers app code is built with the F3 SDK (CC23xx/CC27xx).
 * Every line here is a gap between two TI SDKs that share the TI Drivers API.
 * Counted in portability/README.md. Keep one gap per line. */
#pragma once
/* GPIO: MSPM0 has a high-impedance flag F3 lacks. Dropped: F3 pins default to input with no pull. */
#define GPIO_CFG_HIZ_ON 0
#define GPIO_CFG_HIZ_OFF 0
/* SPI: MSPM0 names 3- and 4-wire Motorola frames; F3 has 4-wire only, without "MOTO4". */
#define SPI_MOTO4_POL0_PHA0 SPI_POL0_PHA0
#define SPI_MOTO4_POL0_PHA1 SPI_POL0_PHA1
#define SPI_MOTO4_POL1_PHA0 SPI_POL1_PHA0
#define SPI_MOTO4_POL1_PHA1 SPI_POL1_PHA1
/* UART: same 13 calls, renamed UART2 on F3. */
#include <ti/drivers/UART2.h>
#define UART_Handle UART2_Handle
#define UART_Params UART2_Params
#define UART_Params_init UART2_Params_init
#define UART_open UART2_open
#define UART_close UART2_close
#define UART_read UART2_read
#define UART_write UART2_write
#define UART_readTimeout UART2_readTimeout
#define UART_writeTimeout UART2_writeTimeout
#define UART_readCancel UART2_readCancel
#define UART_writeCancel UART2_writeCancel
#define UART_flushRx UART2_flushRx
#define UART_rxEnable UART2_rxEnable
#define UART_rxDisable UART2_rxDisable
#define UART_getRxCount UART2_getRxCount
#define UART_STATUS_SUCCESS UART2_STATUS_SUCCESS
#define UART_Mode_BLOCKING UART2_Mode_BLOCKING
#define UART_Mode_CALLBACK UART2_Mode_CALLBACK
#define UART_Mode_NONBLOCKING UART2_Mode_NONBLOCKING
/* UART board names: the MSPM0 board file calls the port CONFIG_UART_0; SysConfig on F3 calls it CONFIG_UART2_0. */
#define CONFIG_UART_0 CONFIG_UART2_0
/* UART settings the MSPM0 board file defines and F3 keeps in SysConfig. Values are the ones in the MSPM0 uart examples. */
#define CONFIG_UART_BAUD_RATE (115200)
#define CONFIG_UART_BUFFER_LENGTH 1
/* LED names: MSPM0 examples say CONFIG_LED_ON / CONFIG_LED_OFF; F3 says CONFIG_GPIO_LED_ON / CONFIG_GPIO_LED_OFF. */
#define CONFIG_LED_ON CONFIG_GPIO_LED_ON
#define CONFIG_LED_OFF CONFIG_GPIO_LED_OFF
/* LED name used by the MSPM0 I2C target example (CONFIG_GPIO_LED_0_ON); F3 has one name for all LEDs. */
#define CONFIG_GPIO_LED_0_ON CONFIG_GPIO_LED_ON
/* I2C target: the MSPM0 example names its target instance CONFIG_I2C_TARGET_0; F3 SysConfig names it CONFIG_I2C_0. */
#define CONFIG_I2C_TARGET_0 CONFIG_I2C_0
/* I2C target: the MSPM0 example's error handler uses the controller types; F3's I2CTarget.h does not pull them in. */
#include <ti/drivers/I2C.h>
/* GPIO: two MSPM0 examples switch the LED with a DriverLib call on the pin group of their board file
 * (GPIOA pin 0, active low). On F3 the same pin is the TI Drivers LED 0. */
#include <ti/drivers/GPIO.h>
#define GPIO_GRP_0_PORT 0
#define GPIO_GRP_0_PIN_0_PIN 0
#define DL_GPIO_clearPins(port, pins) GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON)
