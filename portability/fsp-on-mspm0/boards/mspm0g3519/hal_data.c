/* What the FSP configurator generates (ra_gen/hal_data.c), written by hand for the LP-MSPM0G3519 board:
 * touch controller FT6336U at 0x38 on I2C2, console on UART0, PWM period 1000 counts on PB27 (1 kHz). The timer, ELC and DTC objects of the
 * gpt and adc_gpt_periodic_sampling examples are compiled in only for those examples (build.sh passes -DFSP_EX_<example>). */
#include "hal_data.h"

extern void i2c_master_callback(i2c_master_callback_args_t *p) __attribute__((weak));
extern void user_uart_callback(uart_callback_args_t *p) __attribute__((weak));

i2c_master_ctrl_t g_i2c_master_ctrl;
const i2c_master_cfg_t g_i2c_master_cfg = { 0x38, i2c_master_callback, 0 };
uart_ctrl_t g_uart_ctrl;
const uart_cfg_t g_uart_cfg = { user_uart_callback, 0 };
extern void adc_callback(adc_callback_args_t *p) __attribute__((weak));
adc_ctrl_t g_adc_ctrl;
const adc_cfg_t g_adc_cfg = { ADC_MODE_SINGLE_SCAN, adc_callback, 0 };
const adc_channel_cfg_t g_adc_channel_cfg = { 1u << 0 };
#if defined(FSP_EX_sci_uart)
timer_ctrl_t g_timer_ctrl;
const timer_cfg_t g_timer_cfg = { TIMER_MODE_PWM, 1, 0, 1000, 0, 0 };
#endif

#if defined(FSP_EX_gpt)   /* periodic and one-shot on channel 0 (TIMG12, counts at 32 MHz), PWM on channel 1 (PB27) */
#ifndef ONE_SHOT_COUNTS
#define ONE_SHOT_COUNTS 1000     /* the published setting: 1000 raw counts */
#endif
extern void user_gpt_one_shot_callback(timer_callback_args_t *p) __attribute__((weak));
timer_ctrl_t g_timer_periodic_ctrl, g_timer_pwm_ctrl, g_timer_one_shot_mode_ctrl;
const timer_cfg_t g_timer_periodic_cfg = { TIMER_MODE_PERIODIC, 0, 0, 1000, 0, 0 };
const timer_cfg_t g_timer_pwm_cfg = { TIMER_MODE_PWM, 1, 0, 1000, 0, 0 };
const timer_cfg_t g_timer_one_shot_mode_cfg = { TIMER_MODE_ONE_SHOT, 0, 0, ONE_SHOT_COUNTS, user_gpt_one_shot_callback, 0 };
#endif

#if defined(FSP_EX_adc_gpt_periodic_sampling)   /* GPT0 period 21 us = 672 counts; the ELC links its overflow to ADC0; four DMA channels stand in for the DTC */
extern void g_adc0_callback(adc_callback_args_t *p) __attribute__((weak));
adc_ctrl_t g_adc0_ctrl;
const adc_cfg_t g_adc0_cfg = { ADC_MODE_GROUP_SCAN, g_adc0_callback, 0 };
const adc_channel_cfg_t g_adc0_channel_cfg = { (1u << 4) | (1u << 5), (1u << 6) | (1u << 7) };
timer_ctrl_t g_timer0_ctrl;
const timer_cfg_t g_timer0_cfg = { TIMER_MODE_PERIODIC, 2, 0, 672, 0, 0 };
elc_ctrl_t g_elc_ctrl;
const elc_cfg_t g_elc_cfg = { { ELC_EVENT_GPT0_COUNTER_OVERFLOW } };
static transfer_info_t s_info_a, s_info_b;
transfer_ctrl_t g_transfer_adc0_group_a_ctrl, g_transfer_adc0_group_b_ctrl;
const transfer_cfg_t g_transfer_adc0_group_a_cfg = { &s_info_a, ADC_EVENT_SCAN_COMPLETE };
const transfer_cfg_t g_transfer_adc0_group_b_cfg = { &s_info_b, ADC_EVENT_SCAN_COMPLETE_GROUP_B };
#endif
