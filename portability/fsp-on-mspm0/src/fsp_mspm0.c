/* FSP calls on MSPM0 DriverLib (LP-MSPM0G3519). Names and order of calls follow the public FSP interface; the code is ours.
 * Hardware is set up by SysConfig (ti_msp_dl_config.c): UART_0 on PA10/PA11, TP_I2C = I2C2 on PC2/PC3, PWM_0 = TIMA1 CC1 on PB27, LED on PA0.
 * GPT (src/fsp_mspm0_gpt.c) and the ELC, DTC and ADC group scan (src/fsp_mspm0_adcgpt.c) are in their own files. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "ti_msp_dl_config.h"
#include "hal_data.h"
#include "SEGGER_RTT/SEGGER_RTT.h"
#ifdef FSP_SHIM_TRACE
#define FSP_TRACE(...) SEGGER_RTT_printf(0, __VA_ARGS__)
#else
#define FSP_TRACE(...)
#endif

ioport_ctrl_t g_ioport_ctrl;
const ioport_cfg_t g_bsp_pin_cfg;
fsp_err_t R_FSP_VersionGet(fsp_pack_version_t *v) { v->version_id_b.major = 0; v->version_id_b.minor = 1; v->version_id_b.patch = 0; v->version_id_b.build = 0; return FSP_SUCCESS; }
fsp_err_t R_IOPORT_Open(ioport_ctrl_t *c, const ioport_cfg_t *f) { (void) c; (void) f; return FSP_SUCCESS; }

void R_BSP_SoftwareDelay(uint32_t delay, bsp_delay_units_t units)
{
    uint32_t per = (uint32_t) units;                       /* ticks per second */
    uint32_t cyc = CPUCLK_FREQ / per;                      /* cycles per tick */
    while (delay--) { delay_cycles(cyc); }
}

/* ---- UART0: console and SCI UART share the port ---- */
static uart_cfg_t const *s_ucfg;
static volatile uint8_t s_rb[32]; static volatile uint32_t s_rh, s_rt, s_lines; static volatile uint8_t s_last = '\r';     /* console input ring, used while no SCI UART is open */
static void tx_bytes(const uint8_t *p, uint32_t n) { while (n--) { DL_UART_Main_transmitDataBlocking(UART_0_INST, *p++); } }

int SEGGER_RTT_printf(unsigned idx, const char *fmt, ...)
{
    char b[384]; va_list ap; (void) idx;
#ifdef FSP_SHIM_TRACE
    if (fmt[0] == '[' && fmt[1] == 'E') { char t[96]; int m = snprintf(t, sizeof t, "<st=%x raw=%x mctr=%x msa=%x>", (unsigned) DL_I2C_getControllerStatus(TP_I2C_INST), (unsigned) DL_I2C_getRawInterruptStatus(TP_I2C_INST, 0xFFFFFFFFu), (unsigned) TP_I2C_INST->MASTER.MCTR, (unsigned) TP_I2C_INST->MASTER.MSA); tx_bytes((uint8_t *) t, (uint32_t) m); }
#endif
    va_start(ap, fmt); int n = vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    if (n > (int) sizeof b - 1) { n = sizeof b - 1; }
    if (n > 0) { tx_bytes((uint8_t *) b, (uint32_t) n); }
    return n;
}

/* The RTT console stand-in is line buffered, as typing a line into the RTT viewer is: HasKey is true once a whole line is in, Read returns that line
 * without its end-of-line. (Bytes handed over as they arrive made "25" read as "2", then "5".) */
unsigned SEGGER_RTT_Read(unsigned idx, void *buf, unsigned n)
{
    unsigned k = 0; (void) idx;
    if (!s_lines) { return 0; }
    while (s_rt != s_rh) {
        uint8_t ch = s_rb[s_rt++ % 32];
        if (ch == '\r') { s_lines--; break; }
        if (k < n) { ((uint8_t *) buf)[k++] = ch; }
    }
    return k;
}
#ifdef FSP_SHIM_MEASURE
void fsp_measure_service(void);
#endif
int SEGGER_RTT_HasKey(void)
{
#ifdef FSP_SHIM_MEASURE
    fsp_measure_service();
#endif
    return s_lines != 0;
}

fsp_err_t R_SCI_UART_Open(uart_ctrl_t *c, const uart_cfg_t *f)
{
    if (!c || !f) { return FSP_ERR_ASSERTION; }
    if (c->open) { return FSP_ERR_ALREADY_OPEN; }
    s_ucfg = f; c->open = true;
    DL_UART_Main_enableInterrupt(UART_0_INST, DL_UART_MAIN_INTERRUPT_RX);
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
    return FSP_SUCCESS;
}
fsp_err_t R_SCI_UART_Write(uart_ctrl_t *c, uint8_t const *p, uint32_t n)
{
    if (!c || !p) { return FSP_ERR_ASSERTION; }
    if (!c->open) { return FSP_ERR_NOT_OPEN; }
    tx_bytes(p, n);
    if (s_ucfg->p_callback) { uart_callback_args_t a = { 0, UART_EVENT_TX_COMPLETE, 0, s_ucfg->p_context }; s_ucfg->p_callback(&a); }
    return FSP_SUCCESS;
}
fsp_err_t R_SCI_UART_Close(uart_ctrl_t *c)
{
    if (!c) { return FSP_ERR_ASSERTION; }
    DL_UART_Main_disableInterrupt(UART_0_INST, DL_UART_MAIN_INTERRUPT_RX); NVIC_DisableIRQ(UART_0_INST_INT_IRQN); c->open = false;
    return FSP_SUCCESS;
}
void UART_0_INST_IRQHandler(void)
{
    while (!DL_UART_Main_isRXFIFOEmpty(UART_0_INST)) {
        uint8_t ch = DL_UART_Main_receiveData(UART_0_INST);
        if (s_ucfg && s_ucfg->p_callback) { uart_callback_args_t a = { 0, UART_EVENT_RX_CHAR, ch, s_ucfg->p_context }; s_ucfg->p_callback(&a); }
        else if (ch == '\n') { }
        else if (ch != '\r' || s_last != '\r') { s_rb[s_rh++ % 32] = ch; if (ch == '\r') { s_lines++; } s_last = ch; }
    }
    DL_UART_Main_clearInterruptStatus(UART_0_INST, DL_UART_MAIN_INTERRUPT_RX);
}

/* ---- IIC master on I2C2 ---- */
static i2c_master_cfg_t const *s_icfg;
static void i2c_event(i2c_master_event_t e, uint32_t n)
{
    if (s_icfg && s_icfg->p_callback) { i2c_master_callback_args_t a = { n, e, s_icfg->p_context }; s_icfg->p_callback(&a); }
}
fsp_err_t R_IIC_MASTER_Open(i2c_master_ctrl_t *c, const i2c_master_cfg_t *f)
{
    if (!c || !f) { return FSP_ERR_ASSERTION; }
    if (c->open) { return FSP_ERR_ALREADY_OPEN; }
    s_icfg = f; c->open = true; c->wait_stop = false;
    DL_I2C_enableInterrupt(TP_I2C_INST, DL_I2C_INTERRUPT_CONTROLLER_TX_DONE | DL_I2C_INTERRUPT_CONTROLLER_RX_DONE |
                           DL_I2C_INTERRUPT_CONTROLLER_NACK | DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST);
    NVIC_EnableIRQ(TP_I2C_INST_INT_IRQN);
    return FSP_SUCCESS;
}
static fsp_err_t i2c_start(i2c_master_ctrl_t *c, uint32_t n, bool restart)
{
    if (!c) { return FSP_ERR_ASSERTION; }
    if (!c->open) { return FSP_ERR_NOT_OPEN; }
    if (n == 0 || n > 8) { return FSP_ERR_INVALID_ARGUMENT; }       /* one FIFO fill; the examples move 1 to 6 bytes */
    if (!c->wait_stop) { while (!(DL_I2C_getControllerStatus(TP_I2C_INST) & DL_I2C_CONTROLLER_STATUS_IDLE)) { } }
    DL_I2C_flushControllerTXFIFO(TP_I2C_INST);
    c->wait_stop = restart; c->len = n;
    return FSP_SUCCESS;
}
fsp_err_t R_IIC_MASTER_Write(i2c_master_ctrl_t *c, uint8_t *p, uint32_t n, bool restart)
{
    fsp_err_t e = i2c_start(c, n, restart);
    if (e != FSP_SUCCESS) { return e; }
    DL_I2C_fillControllerTXFIFO(TP_I2C_INST, p, n);
    DL_I2C_startControllerTransferAdvanced(TP_I2C_INST, s_icfg->slave, DL_I2C_CONTROLLER_DIRECTION_TX, n,
        DL_I2C_CONTROLLER_START_ENABLE, restart ? DL_I2C_CONTROLLER_STOP_DISABLE : DL_I2C_CONTROLLER_STOP_ENABLE, DL_I2C_CONTROLLER_ACK_DISABLE);   /* ACK bit off: the controller NACKs the last byte of a read */
    return FSP_SUCCESS;
}
fsp_err_t R_IIC_MASTER_Read(i2c_master_ctrl_t *c, uint8_t *p, uint32_t n, bool restart)
{
    fsp_err_t e = i2c_start(c, n, restart);
    if (e != FSP_SUCCESS) { return e; }
    c->rx = p;
    DL_I2C_startControllerTransferAdvanced(TP_I2C_INST, s_icfg->slave, DL_I2C_CONTROLLER_DIRECTION_RX, n,
        DL_I2C_CONTROLLER_START_ENABLE, restart ? DL_I2C_CONTROLLER_STOP_DISABLE : DL_I2C_CONTROLLER_STOP_ENABLE, DL_I2C_CONTROLLER_ACK_DISABLE);   /* ACK bit off: the controller NACKs the last byte of a read */
    return FSP_SUCCESS;
}
fsp_err_t R_IIC_MASTER_Close(i2c_master_ctrl_t *c)
{
    if (!c) { return FSP_ERR_ASSERTION; }
    DL_I2C_disableInterrupt(TP_I2C_INST, 0xFFFFFFFFu); NVIC_DisableIRQ(TP_I2C_INST_INT_IRQN); c->open = false;
    return FSP_SUCCESS;
}
void TP_I2C_INST_IRQHandler(void)
{
    uint32_t iidx = DL_I2C_getPendingInterrupt(TP_I2C_INST);
    FSP_TRACE("[i2c irq %u st=%x]", (unsigned) iidx, (unsigned) DL_I2C_getControllerStatus(TP_I2C_INST));
    switch (iidx) {
        case DL_I2C_IIDX_CONTROLLER_TX_DONE: i2c_event(I2C_MASTER_EVENT_TX_COMPLETE, g_i2c_master_ctrl.len); break;
        case DL_I2C_IIDX_CONTROLLER_RX_DONE:
            for (uint32_t i = 0; i < g_i2c_master_ctrl.len; i++) { g_i2c_master_ctrl.rx[i] = DL_I2C_receiveControllerData(TP_I2C_INST); }
            i2c_event(I2C_MASTER_EVENT_RX_COMPLETE, g_i2c_master_ctrl.len); break;
        case DL_I2C_IIDX_CONTROLLER_NACK:
        case DL_I2C_IIDX_CONTROLLER_ARBITRATION_LOST: i2c_event(I2C_MASTER_EVENT_ABORTED, 0); break;
        default: break;
    }
}

/* ---- ADC0: configured by SysConfig. Single scan: one conversion on channel 0, Read waits for it. Group scan: src/fsp_mspm0_adcgpt.c ---- */
#ifdef FSP_ADC_GROUP   /* group scan on a timer event with DMA: src/fsp_mspm0_adcgpt.c */
void adc_group_open(const adc_cfg_t *f); void adc_group_scancfg(const adc_channel_cfg_t *f); void adc_group_start(void); void adc_group_stop(void);
#endif
fsp_err_t R_ADC_Open(adc_ctrl_t *c, const adc_cfg_t *f)
{
    if (!c || !f) { return FSP_ERR_ASSERTION; }
    if (c->open) { return FSP_ERR_ALREADY_OPEN; }
    c->open = true; c->group = (f->mode == ADC_MODE_GROUP_SCAN);
#ifdef FSP_ADC_GROUP
    if (c->group) { adc_group_open(f); }
#endif
    return FSP_SUCCESS;
}
fsp_err_t R_ADC_ScanCfg(adc_ctrl_t *c, const adc_channel_cfg_t *f)
{
    if (!c || !f) { return FSP_ERR_ASSERTION; }
    if (!c->open) { return FSP_ERR_NOT_OPEN; }
#ifdef FSP_ADC_GROUP
    if (c->group) { adc_group_scancfg(f); }
#endif
    return FSP_SUCCESS;
}
fsp_err_t R_ADC_ScanStart(adc_ctrl_t *c)
{
    if (!c) { return FSP_ERR_ASSERTION; }
    if (!c->open) { return FSP_ERR_NOT_OPEN; }
#ifdef FSP_ADC_GROUP
    if (c->group) { adc_group_start(); c->scanning = true; return FSP_SUCCESS; }   /* conversions wait for the timer event */
#endif
    DL_ADC12_enableConversions(ADC12_0_INST); DL_ADC12_startConversion(ADC12_0_INST); c->scanning = true; return FSP_SUCCESS;
}
fsp_err_t R_ADC_ScanStop(adc_ctrl_t *c)
{
    if (!c) { return FSP_ERR_ASSERTION; }
#ifdef FSP_ADC_GROUP
    if (c->group) { adc_group_stop(); c->scanning = false; return FSP_SUCCESS; }
#endif
    DL_ADC12_stopConversion(ADC12_0_INST); c->scanning = false; return FSP_SUCCESS;
}
fsp_err_t R_ADC_Read(adc_ctrl_t *c, adc_channel_t ch, uint16_t *const d)
{
    if (!c || !d) { return FSP_ERR_ASSERTION; }
    if (!c->open) { return FSP_ERR_NOT_OPEN; }
    if (ch != ADC_CHANNEL_0) { return FSP_ERR_INVALID_ARGUMENT; }
    while (!DL_ADC12_getRawInterruptStatus(ADC12_0_INST, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED)) { }
    *d = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0);
    DL_ADC12_clearInterruptStatus(ADC12_0_INST, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
    return FSP_SUCCESS;
}
fsp_err_t R_ADC_Close(adc_ctrl_t *c)
{
    if (!c) { return FSP_ERR_ASSERTION; }
#ifdef FSP_ADC_GROUP
    if (c->group) { adc_group_stop(); }
#endif
    c->open = false; c->scanning = false; return FSP_SUCCESS;
}
void R_BSP_IrqDisable(IRQn_Type irq) { (void) irq; }
IRQn_Type R_FSP_CurrentIrqGet(void) { return (IRQn_Type) 0; }

/* ---- startup: what FSP's generated main() does ---- */
void R_BSP_WarmStart(bsp_warm_start_event_t event) __attribute__((weak));
int main(void)
{
    SYSCFG_DL_init();
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);                          /* console input (SEGGER_RTT_Read) */
    /* Waveshare touch controller (FT6336U): hold its reset low 5 ms, release, and wait 400 ms for it to boot. Before this the first
     * I2C transfer finished 65 ms late (the controller keeps the bus busy while it starts), which the example's 16 ms wait reads as a failed write. */
    DL_GPIO_clearPins(PB_PORT, PB_TP_RST_PIN); delay_cycles(CPUCLK_FREQ / 200);
    DL_GPIO_setPins(PB_PORT, PB_TP_RST_PIN); delay_cycles(CPUCLK_FREQ / 1000 * 400);
    if (R_BSP_WarmStart) { R_BSP_WarmStart(BSP_WARM_START_POST_C); }
    hal_entry();
    for (;;) { }
}
