/* Pico SDK on MSPM0, LP-MSPM0G3519: the board defaults the Pico SDK's board header gives, in Pico GPIO numbers.
 * GP0/GP1 = PA10/PA11 (UART0, probe serial port), GP4/GP5 = PC3/PC2 (I2C2), GP25 = PB27 (green LED, TIMA1 channel 1),
 * GP26 = PA27 (ADC0 channel 0). The table itself is pins.c. */
#ifndef PICO_MSPM0_BOARD_H
#define PICO_MSPM0_BOARD_H
#define PICO_DEFAULT_UART 0
#define PICO_DEFAULT_UART_TX_PIN 0
#define PICO_DEFAULT_UART_RX_PIN 1
#define PICO_DEFAULT_I2C 0
#define PICO_DEFAULT_I2C_SDA_PIN 4
#define PICO_DEFAULT_I2C_SCL_PIN 5
#define PICO_DEFAULT_LED_PIN 25
#define PICO_MSPM0_PWM_IRQN 19          /* TIMA1_INT_IRQn: PWM slice 0 */
#endif
