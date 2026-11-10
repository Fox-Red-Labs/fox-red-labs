/* Written by gen_boards.py from its pin table: do not edit by hand. BSD-3-Clause. */
#ifndef HOBBY_PINS_H
#define HOBBY_PINS_H

#define HB_HAS_BTN_A 1
#define HB_PORT_BTN_A GPIO_BTN_A_PORT
#define HB_PIN_BTN_A  GPIO_BTN_A_BTN_A_PIN
#define HB_HAS_BTN_B 1
#define HB_PORT_BTN_B GPIO_BTN_B_PORT
#define HB_PIN_BTN_B  GPIO_BTN_B_BTN_B_PIN
#define HB_HAS_LED 1
#define HB_PWM_INST_LED PWM_LED_INST
#define HB_PWM_IDX_LED  DL_TIMER_CC_0_INDEX
#define HB_PWM_CLK_LED  PWM_LED_INST_CLK_FREQ
#define HB_HAS_POT1 1
#define HB_ADCMEM_POT1 0

#endif
