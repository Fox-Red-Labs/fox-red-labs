/* TI-side stand-in for ST's Inc/main.h (NUCLEO-U083RC UART_Printf): same name, so ST's main.c compiles with
   the edits listed in ../README.md. Target: STM32G4 HAL compatibility layer, SDK 26.01.00.03. */
#ifndef __MAIN_H
#define __MAIN_H
#include <stdio.h>
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_uart.h"
#include "ti_sdk_dl_config.h"
#undef  USART2
#define USART2                 UART5            /* the layer maps USART2 to UC1; the backchannel is UC4 = UART5 */
#define LED4_PIN               GPIO_PIN_5       /* LD4 is PA5 on the NUCLEO-U083RC; LED4 is PA5 on the LP-AM13E230 */
#define LED4_GPIO_PORT         GPIOA
#define __HAL_RCC_GPIOA_CLK_ENABLE() /* the layer has no clock gates to open */
typedef enum { LED4 = 0 } Led_TypeDef;
#define BSP_LED_Init(l)        do { GPIO_InitTypeDef g_ = {0}; g_.Pin = LED4_PIN; g_.Mode = GPIO_MODE_OUTPUT_PP; HAL_GPIO_Init(LED4_GPIO_PORT, &g_); } while (0)
#define BSP_LED_Toggle(l)      HAL_GPIO_TogglePin(LED4_GPIO_PORT, LED4_PIN)
#define HAL_Delay(ms)          HAL_Delay((ms) * (CPUCLK_FREQ_HZ / 1000U)) /* the layer's HAL_Delay counts CPU cycles, ST's counts ms */
void Error_Handler(void);
#endif
