/* TI-side stand-in for ST's Inc/main.h (NUCLEO-U083RC GPIO_IOToggle): same name, so ST's main.c compiles unchanged
   apart from the edits listed in ../README.md. Target: STM32G4 HAL compatibility layer, SDK 26.01.00.03. */
#ifndef __MAIN_H
#define __MAIN_H
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_gpio.h"
#include "ti_sdk_dl_config.h"
#define LED4_PIN              GPIO_PIN_5        /* LD4 is PA5 on the NUCLEO-U083RC and LED4 is PA5 on the LP-AM13E230 */
#define LED4_GPIO_PORT        GPIOA
#define LED4_GPIO_CLK_ENABLE() /* the layer has no clock gates to open */
#define HAL_Delay(ms)         HAL_Delay((ms) * (CPUCLK_FREQ_HZ / 1000U)) /* the layer's HAL_Delay counts CPU cycles, ST's counts ms */
void Error_Handler(void);
#endif
