/* Replaces ST's Src/stm32u0xx_hal_msp.c (RCC clock select, USART2 gate, GPIO alternate function 7), which has no
   equivalent in the layer. Same as TI's own UART_Printf msp: UART5 on PA0 and PA1, the LaunchPad backchannel. */
#include "main.h"
#include <driverlib.h>
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef g = {0};
    if (huart->Instance == UART5) {
        DL_UART_reset(UC4_INST_PTR);
        DL_UART_enablePower(UC4_INST_PTR);
        g.Pin = GPIO_PIN_0; g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
        g.Alternate = IOMUX_PA0_UC4_TX_SDA_PICO; HAL_GPIO_Init(GPIOA, &g);
        g.Pin = GPIO_PIN_1; g.Alternate = IOMUX_PA1_UC4_RX_SCL_SCLK; HAL_GPIO_Init(GPIOA, &g);
    }
}
void HAL_UART_MspDeInit(UART_HandleTypeDef *huart) { if (huart->Instance == UART5) HAL_GPIO_DeInit(GPIOA, GPIO_PIN_0 | GPIO_PIN_1); }
void HAL_MspInit(void) {}
