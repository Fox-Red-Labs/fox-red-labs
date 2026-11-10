/* Written by gen_boards.py from its pin table: do not edit by hand. BSD-3-Clause. */
#ifndef HOBBY_PINS_H
#define HOBBY_PINS_H

#define HB_HAS_TRIG 1
#define HB_PORT_TRIG GPIO_TRIG_PORT
#define HB_PIN_TRIG  GPIO_TRIG_TRIG_PIN
#define HB_HAS_ECHO 1
#define HB_PORT_ECHO GPIO_ECHO_PORT
#define HB_PIN_ECHO  GPIO_ECHO_ECHO_PIN
#define HB_HAS_BUZZ 1
#define HB_PWM_INST_BUZZ PWM_BUZZ_INST
#define HB_PWM_IDX_BUZZ  DL_TIMER_CC_1_INDEX
#define HB_PWM_CLK_BUZZ  PWM_BUZZ_INST_CLK_FREQ

#endif
