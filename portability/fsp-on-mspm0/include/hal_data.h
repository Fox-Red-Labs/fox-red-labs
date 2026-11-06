/* FSP names on TI DriverLib: the types and calls the Renesas ra-fsp-examples use, written for the MSPM0 (portability/fsp-on-mspm0).
 * Names and call order follow the public FSP driver interface; the code behind them is ours. Only what the ported examples call. */
#ifndef HAL_DATA_H_
#define HAL_DATA_H_
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <ti/devices/msp/msp.h>   /* IRQn_Type, which the FSP examples name in a cast */

typedef enum e_fsp_err {
    FSP_SUCCESS = 0, FSP_ERR_ASSERTION = 1, FSP_ERR_INVALID_POINTER = 2, FSP_ERR_INVALID_ARGUMENT = 3,
    FSP_ERR_ALREADY_OPEN = 4, FSP_ERR_NOT_OPEN = 5, FSP_ERR_TIMEOUT = 6, FSP_ERR_TRANSFER_ABORTED = 7, FSP_ERR_IN_USE = 8,
    FSP_ERR_UNSUPPORTED = 9, FSP_ERR_ABORTED = 10
} fsp_err_t;

typedef struct { union { uint32_t version_id; struct { uint8_t patch; uint8_t minor; uint8_t major; uint8_t build; } version_id_b; }; } fsp_pack_version_t;
fsp_err_t R_FSP_VersionGet(fsp_pack_version_t *p_version);
#define FSP_CPP_HEADER
#define FSP_CPP_FOOTER
typedef enum { FSP_PRIV_CLOCK_PCLKD = 3 } fsp_priv_clock_t;
uint32_t R_FSP_SystemClockHzGet(fsp_priv_clock_t clock);   /* the clock the timers count: 32 MHz */

/* BSP */
typedef enum { BSP_WARM_START_RESET = 0, BSP_WARM_START_POST_C = 1 } bsp_warm_start_event_t;
typedef enum { BSP_DELAY_UNITS_SECONDS = 1, BSP_DELAY_UNITS_MILLISECONDS = 1000, BSP_DELAY_UNITS_MICROSECONDS = 1000000 } bsp_delay_units_t;
void R_BSP_SoftwareDelay(uint32_t delay, bsp_delay_units_t units);

/* IOPORT: the pins are set by SysConfig's generated code; Open only records the call. */
typedef struct { uint8_t unused; } ioport_ctrl_t;
typedef struct { uint8_t unused; } ioport_cfg_t;
extern ioport_ctrl_t g_ioport_ctrl;
extern const ioport_cfg_t g_bsp_pin_cfg;
fsp_err_t R_IOPORT_Open(ioport_ctrl_t *p_ctrl, const ioport_cfg_t *p_cfg);

/* IIC master (blocking transfer started in the call is finished in the interrupt, and the callback runs there, as in FSP) */
typedef enum { I2C_MASTER_EVENT_ABORTED = 1, I2C_MASTER_EVENT_RX_COMPLETE = 2, I2C_MASTER_EVENT_TX_COMPLETE = 3 } i2c_master_event_t;
typedef struct { uint32_t bytes; i2c_master_event_t event; void const *p_context; } i2c_master_callback_args_t;
typedef struct { bool open; uint8_t *rx; uint32_t len; bool wait_stop; } i2c_master_ctrl_t;
typedef struct { uint16_t slave; void (*p_callback)(i2c_master_callback_args_t *); void const *p_context; } i2c_master_cfg_t;
extern i2c_master_ctrl_t g_i2c_master_ctrl;
extern const i2c_master_cfg_t g_i2c_master_cfg;
fsp_err_t R_IIC_MASTER_Open(i2c_master_ctrl_t *p_ctrl, const i2c_master_cfg_t *p_cfg);
fsp_err_t R_IIC_MASTER_Write(i2c_master_ctrl_t *p_ctrl, uint8_t *p_src, uint32_t bytes, bool restart);
fsp_err_t R_IIC_MASTER_Read(i2c_master_ctrl_t *p_ctrl, uint8_t *p_dest, uint32_t bytes, bool restart);
fsp_err_t R_IIC_MASTER_Close(i2c_master_ctrl_t *p_ctrl);

/* SCI UART */
typedef enum {
    UART_EVENT_RX_COMPLETE = 1u << 0, UART_EVENT_TX_COMPLETE = 1u << 1, UART_EVENT_RX_CHAR = 1u << 2, UART_EVENT_ERR_PARITY = 1u << 3,
    UART_EVENT_ERR_FRAMING = 1u << 4, UART_EVENT_ERR_OVERFLOW = 1u << 5, UART_EVENT_BREAK_DETECT = 1u << 6, UART_EVENT_TX_DATA_EMPTY = 1u << 7
} uart_event_t;
typedef struct { uint32_t channel; uart_event_t event; uint32_t data; void const *p_context; } uart_callback_args_t;
typedef struct { bool open; } uart_ctrl_t;
typedef struct { void (*p_callback)(uart_callback_args_t *); void const *p_context; } uart_cfg_t;
extern uart_ctrl_t g_uart_ctrl;
extern const uart_cfg_t g_uart_cfg;
fsp_err_t R_SCI_UART_Open(uart_ctrl_t *p_ctrl, const uart_cfg_t *p_cfg);
fsp_err_t R_SCI_UART_Write(uart_ctrl_t *p_ctrl, uint8_t const *p_src, uint32_t bytes);
fsp_err_t R_SCI_UART_Close(uart_ctrl_t *p_ctrl);

/* GPT: periodic, one-shot and PWM. Channel 0 = TIMG12 (32 bit), 1 = TIMA1 CC1 on PB27 (PWM), 2 = TIMG0 (16 bit, the ADC trigger) */
typedef enum { GPT_IO_PIN_GTIOCA = 1, GPT_IO_PIN_GTIOCB = 2, GPT_IO_PIN_GTIOCA_AND_GTIOCB = 3 } gpt_io_pin_t;
typedef enum { TIMER_MODE_PERIODIC = 0, TIMER_MODE_ONE_SHOT = 1, TIMER_MODE_PWM = 2 } timer_mode_t;
typedef enum { TIMER_DIRECTION_DOWN = 0, TIMER_DIRECTION_UP = 1 } timer_direction_t;
typedef enum { TIMER_EVENT_CYCLE_END = 0, TIMER_EVENT_CREST = 1 } timer_event_t;
typedef struct { timer_event_t event; uint32_t capture; void const *p_context; } timer_callback_args_t;
typedef struct { timer_direction_t count_direction; uint32_t clock_frequency; uint32_t period_counts; } timer_info_t;
typedef struct { bool open; uint8_t channel; uint32_t period_counts; } timer_ctrl_t;
typedef struct { timer_mode_t mode; uint8_t channel; uint8_t source_div; uint32_t period_counts; void (*p_callback)(timer_callback_args_t *); void const *p_context; } timer_cfg_t;
extern timer_ctrl_t g_timer_ctrl, g_timer_periodic_ctrl, g_timer_pwm_ctrl, g_timer_one_shot_mode_ctrl, g_timer0_ctrl;
extern const timer_cfg_t g_timer_cfg, g_timer_periodic_cfg, g_timer_pwm_cfg, g_timer_one_shot_mode_cfg, g_timer0_cfg;
fsp_err_t R_GPT_Open(timer_ctrl_t *p_ctrl, const timer_cfg_t *p_cfg);
fsp_err_t R_GPT_Start(timer_ctrl_t *p_ctrl);
fsp_err_t R_GPT_Stop(timer_ctrl_t *p_ctrl);
fsp_err_t R_GPT_PeriodSet(timer_ctrl_t *p_ctrl, uint32_t period_counts);
fsp_err_t R_GPT_DutyCycleSet(timer_ctrl_t *p_ctrl, uint32_t duty_cycle_counts, uint32_t pin);
fsp_err_t R_GPT_InfoGet(timer_ctrl_t *p_ctrl, timer_info_t *p_info);
fsp_err_t R_GPT_Close(timer_ctrl_t *p_ctrl);

/* ADC: ADC0 channel 0 (PA27), single conversion, 12 bit against VDDA */
typedef enum { ADC_MODE_SINGLE_SCAN = 0, ADC_MODE_CONTINUOUS_SCAN = 2, ADC_MODE_GROUP_SCAN = 4 } adc_mode_t;
typedef enum { ADC_CHANNEL_0 = 0, ADC_CHANNEL_1 = 1 } adc_channel_t;
typedef enum { ADC_EVENT_SCAN_COMPLETE = 0, ADC_EVENT_WINDOW_COMPARE_A = 1, ADC_EVENT_WINDOW_COMPARE_B = 2, ADC_EVENT_SCAN_COMPLETE_GROUP_B = 3 } adc_event_t;
typedef struct { adc_event_t event; void const *p_context; } adc_callback_args_t;
typedef struct { bool open; bool scanning; bool group; } adc_ctrl_t;
typedef struct { adc_mode_t mode; void (*p_callback)(adc_callback_args_t *); void const *p_context; } adc_cfg_t;
typedef struct { uint32_t scan_mask; uint32_t scan_mask_group_b; } adc_channel_cfg_t;
extern adc_ctrl_t g_adc_ctrl;
extern const adc_cfg_t g_adc_cfg;
extern const adc_channel_cfg_t g_adc_channel_cfg;
fsp_err_t R_ADC_Open(adc_ctrl_t *p_ctrl, const adc_cfg_t *p_cfg);
fsp_err_t R_ADC_ScanCfg(adc_ctrl_t *p_ctrl, const adc_channel_cfg_t *p_channel_cfg);
fsp_err_t R_ADC_ScanStart(adc_ctrl_t *p_ctrl);
fsp_err_t R_ADC_ScanStop(adc_ctrl_t *p_ctrl);
fsp_err_t R_ADC_Read(adc_ctrl_t *p_ctrl, adc_channel_t reg_id, uint16_t *const p_data);
fsp_err_t R_ADC_Close(adc_ctrl_t *p_ctrl);

/* Group scan on a timer trigger. FSP names the ADC result words R_ADC0->ADDR[n]; here they are MEMRES[n] of ADC0, so the DTC
 * source addresses in the example work as written. Channel n of the scan mask is MEMn, converting input channel n-4 (PA27, PA26, PA25, PA24). */
typedef struct { volatile uint32_t ADDR[24]; } r_adc_t;
#define R_ADC0 ((r_adc_t *) (ADC0_BASE + offsetof(ADC12_Regs, ULLMEM.MEMRES)))
extern const adc_cfg_t g_adc0_cfg;
extern adc_ctrl_t g_adc0_ctrl;
extern const adc_channel_cfg_t g_adc0_channel_cfg;

/* ELC: a table of links, event to peripheral. MSPM0 event fabric: the timer publishes on channel 1, the ADC subscribes to it. */
typedef enum { ELC_EVENT_NONE = 0, ELC_EVENT_GPT0_COUNTER_OVERFLOW = 1 } elc_event_t;
typedef enum { ELC_PERIPHERAL_ADC0 = 0, ELC_PERIPHERAL_NUM = 1 } elc_peripheral_t;
typedef struct { bool open; } elc_ctrl_t;
typedef struct { elc_event_t link[ELC_PERIPHERAL_NUM]; } elc_cfg_t;
extern elc_ctrl_t g_elc_ctrl;
extern const elc_cfg_t g_elc_cfg;
fsp_err_t R_ELC_Open(elc_ctrl_t *p_ctrl, const elc_cfg_t *p_cfg);
fsp_err_t R_ELC_Enable(elc_ctrl_t *p_ctrl);
fsp_err_t R_ELC_Close(elc_ctrl_t *p_ctrl);

/* DTC: transfer_info_t as FSP lays it out. Each element becomes one MSPM0 DMA channel, all triggered by the ADC result event. */
typedef enum { TRANSFER_SIZE_1_BYTE = 0, TRANSFER_SIZE_2_BYTE = 1, TRANSFER_SIZE_4_BYTE = 2 } transfer_size_t;
typedef enum { TRANSFER_MODE_NORMAL = 0, TRANSFER_MODE_REPEAT = 1, TRANSFER_MODE_BLOCK = 2 } transfer_mode_t;
typedef enum { TRANSFER_ADDR_MODE_FIXED = 0, TRANSFER_ADDR_MODE_OFFSET = 1, TRANSFER_ADDR_MODE_INCREMENTED = 2, TRANSFER_ADDR_MODE_DECREMENTED = 3 } transfer_addr_mode_t;
typedef enum { TRANSFER_IRQ_END = 0, TRANSFER_IRQ_EACH = 1 } transfer_irq_t;
typedef enum { TRANSFER_CHAIN_MODE_DISABLED = 0, TRANSFER_CHAIN_MODE_EACH = 2, TRANSFER_CHAIN_MODE_END = 3 } transfer_chain_mode_t;
typedef enum { TRANSFER_REPEAT_AREA_DESTINATION = 0, TRANSFER_REPEAT_AREA_SOURCE = 1 } transfer_repeat_area_t;
typedef struct st_transfer_info {
    union { uint32_t transfer_settings_word;
            struct { uint32_t : 16; uint32_t size : 2; uint32_t mode : 2; uint32_t src_addr_mode : 2; uint32_t irq : 2; uint32_t chain_mode : 2; uint32_t dest_addr_mode : 2; uint32_t repeat_area : 1; uint32_t : 3; } transfer_settings_word_b; };
    void const * volatile p_src; void * volatile p_dest; volatile uint16_t num_blocks; volatile uint16_t length;
} transfer_info_t;
typedef struct { bool open; uint8_t first_channel; uint8_t count; adc_event_t event; transfer_info_t *p_info; } transfer_ctrl_t;
typedef struct { transfer_info_t *p_info; adc_event_t event; } transfer_cfg_t;     /* event: what the ADC callback reports when this transfer ends */
extern transfer_ctrl_t g_transfer_adc0_group_a_ctrl, g_transfer_adc0_group_b_ctrl;
extern const transfer_cfg_t g_transfer_adc0_group_a_cfg, g_transfer_adc0_group_b_cfg;
fsp_err_t R_DTC_Open(transfer_ctrl_t *p_ctrl, const transfer_cfg_t *p_cfg);
fsp_err_t R_DTC_Reconfigure(transfer_ctrl_t *p_ctrl, transfer_info_t *p_info);
fsp_err_t R_DTC_Enable(transfer_ctrl_t *p_ctrl);
fsp_err_t R_DTC_Close(transfer_ctrl_t *p_ctrl);
void R_BSP_IrqDisable(IRQn_Type irq);
IRQn_Type R_FSP_CurrentIrqGet(void);

void hal_entry(void);
#endif
