/* Port of the BP-BASSENSORSMKII drivers to MSPM0 and MSPM33 DriverLib. One file for every board; the per-board part is
 * boards/<board>/bassens.syscfg (pins), generated from boards/gen_syscfg.py. SysConfig names used here:
 *   GPIO groups HDC_PWR, DRV_PWR, OPT_PWR, TMP_PWR (one pin each, named HDC, DRV, OPT, TMP), I2C instance "I2C",
 *   UART instance "UART_0", ADC instance "ADC12_0" (absent on MSPM33: the DRV5055 read returns -1 there). */
#include "ti_msp_dl_config.h"
#include "bs_port.h"
#include "bs_board.h"

/* The G3x devices name the I2C controller calls DL_I2C_*Controller*; the UNICOMM devices (G5187, MSPM33) name them DL_I2CC_*. */
#ifdef DL_I2C_CONTROLLER_STATUS_IDLE
#define I2C_ST_IDLE DL_I2C_CONTROLLER_STATUS_IDLE
#define I2C_ST_BUSY DL_I2C_CONTROLLER_STATUS_BUSY
#define I2C_ST_ERR  DL_I2C_CONTROLLER_STATUS_ERROR
#define I2C_DIR_TX  DL_I2C_CONTROLLER_DIRECTION_TX
#define I2C_DIR_RX  DL_I2C_CONTROLLER_DIRECTION_RX
#define i2c_status()        DL_I2C_getControllerStatus(I2C_INST)
#define i2c_flush_tx()      DL_I2C_flushControllerTXFIFO(I2C_INST)
#define i2c_fill_tx(d, n)   DL_I2C_fillControllerTXFIFO(I2C_INST, (d), (n))
#define i2c_start(a, dir, n) DL_I2C_startControllerTransfer(I2C_INST, (a), (dir), (n))
#define i2c_rx_empty()      DL_I2C_isControllerRXFIFOEmpty(I2C_INST)
#define i2c_rx_byte()       DL_I2C_receiveControllerData(I2C_INST)
#else
#define I2C_ST_IDLE DL_I2CC_STATUS_IDLE
#define I2C_ST_BUSY DL_I2CC_STATUS_BUSY
#define I2C_ST_ERR  DL_I2CC_STATUS_ERROR
#define I2C_DIR_TX  DL_I2CC_DIRECTION_TX
#define I2C_DIR_RX  DL_I2CC_DIRECTION_RX
#define i2c_status()        DL_I2CC_getStatus(I2C_INST)
#define i2c_flush_tx()      DL_I2CC_flushTXFIFO(I2C_INST)
#define i2c_fill_tx(d, n)   DL_I2CC_fillTXFIFO(I2C_INST, (d), (n))
#define i2c_start(a, dir, n) DL_I2CC_startTransfer(I2C_INST, (a), (dir), (n))
#define i2c_rx_empty()      DL_I2CC_isRXFIFOEmpty(I2C_INST)
#define i2c_rx_byte()       DL_I2CC_receiveData(I2C_INST)
#endif

static volatile uint32_t ms_ticks;
void SysTick_Handler(void) { ms_ticks++; }

uint32_t bs_port_micros(void)
{
    uint32_t m1, m2, v;
    do { m1 = ms_ticks; v = SysTick->VAL; m2 = ms_ticks; } while (m1 != m2);
    return m1 * 1000u + (SysTick->LOAD - v) / (CPUCLK_FREQ / 1000000u);
}

void bs_port_delay_ms(uint32_t ms)
{
    uint32_t t0 = bs_port_micros();
    while ((uint32_t)(bs_port_micros() - t0) < ms * 1000u) { }
}

static void put(GPIO_Regs *p, uint32_t pin, int level) { if (level) DL_GPIO_setPins(p, pin); else DL_GPIO_clearPins(p, pin); }

/* A power group missing from the SysConfig file (MSPM33C321A: header pins 24, 27 and 28 are not GPIO pads there) is skipped:
 * wire the pack's top pin instead (see PORTING-NOTES.md). */
void bs_port_pin_write(bs_pin_t pin, int level)
{
    switch (pin) {
#ifdef HDC_PWR_PORT
    case BS_PIN_HDC_PWR: put(HDC_PWR_PORT, HDC_PWR_HDC_PIN, level); break;
#endif
#ifdef DRV_PWR_PORT
    case BS_PIN_DRV_PWR: put(DRV_PWR_PORT, DRV_PWR_DRV_PIN, level); break;
#endif
#ifdef OPT_PWR_PORT
    case BS_PIN_OPT_PWR: put(OPT_PWR_PORT, OPT_PWR_OPT_PIN, level); break;
#endif
#ifdef TMP_PWR_PORT
    case BS_PIN_TMP_PWR: put(TMP_PWR_PORT, TMP_PWR_TMP_PIN, level); break;
#endif
    default: break;    /* LED pins 39, 40 are on J4 and not wired on every LaunchPad */
    }
}

/* ---- I2C, polled. A timeout leaves the bus idle and returns -1. ---- */
static int wait_idle(void)
{
    for (uint32_t i = 0; i < 200000; i++) if (i2c_status() & I2C_ST_IDLE) return 0;
    return -1;
}

int bs_port_i2c_write(uint8_t addr, const uint8_t *d, uint8_t n)
{
    if (wait_idle()) return -1;
    i2c_flush_tx();
    if (n) i2c_fill_tx((uint8_t *)d, n);
    i2c_start(addr, I2C_DIR_TX, n);
    delay_cycles(2000);
    for (uint32_t i = 0; i < 400000 && (i2c_status() & I2C_ST_BUSY); i++) { }
    int err = (i2c_status() & I2C_ST_ERR) != 0;
    wait_idle();
    if (err) i2c_flush_tx();
    delay_cycles(2000);
    return err ? -1 : 0;
}

int bs_port_i2c_read(uint8_t addr, uint8_t *d, uint8_t n)
{
    if (wait_idle()) return -1;
    i2c_start(addr, I2C_DIR_RX, n);
    delay_cycles(2000);
    for (uint8_t i = 0; i < n; i++) {
        uint32_t t;
        for (t = 0; t < 400000 && i2c_rx_empty(); t++) { }
        if (i2c_rx_empty()) return -1;
        d[i] = i2c_rx_byte();
    }
    wait_idle();
    delay_cycles(2000);
    return 0;
}

int32_t bs_port_adc_drv_mv(void)
{
#ifdef ADC12_0_INST
    DL_ADC12_enableConversions(ADC12_0_INST);
    DL_ADC12_startConversion(ADC12_0_INST);
    for (uint32_t t = 0; t < 2000000 && !DL_ADC12_getRawInterruptStatus(ADC12_0_INST, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED); t++) { }
    uint32_t code = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0);
    DL_ADC12_clearInterruptStatus(ADC12_0_INST, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
    return (int32_t)(code * 3300u / 4095u);
#else
    return -1;
#endif
}

void bs_console_putc(char c)
{
    while (DL_UART_Main_isTXFIFOFull(UART_0_INST)) { }
    DL_UART_Main_transmitData(UART_0_INST, (uint8_t)c);
}

/* A sensor holding SDA low (after a reset in the middle of a transfer) is released by nine clocks and a STOP before the I2C module takes the pins. */
static void bus_recover(void)
{
    DL_GPIO_initDigitalInputFeatures(GPIO_I2C_IOMUX_SDA, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(GPIO_I2C_IOMUX_SCL, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_clearPins(GPIO_I2C_SDA_PORT, GPIO_I2C_SDA_PIN);
    DL_GPIO_clearPins(GPIO_I2C_SCL_PORT, GPIO_I2C_SCL_PIN);
    DL_GPIO_disableOutput(GPIO_I2C_SDA_PORT, GPIO_I2C_SDA_PIN);
    for (int i = 0; i < 9; i++) {
        DL_GPIO_enableOutput(GPIO_I2C_SCL_PORT, GPIO_I2C_SCL_PIN); delay_cycles(CPUCLK_FREQ / 20000);
        DL_GPIO_disableOutput(GPIO_I2C_SCL_PORT, GPIO_I2C_SCL_PIN); delay_cycles(CPUCLK_FREQ / 20000);
    }
    DL_GPIO_enableOutput(GPIO_I2C_SDA_PORT, GPIO_I2C_SDA_PIN); delay_cycles(CPUCLK_FREQ / 20000);
    DL_GPIO_disableOutput(GPIO_I2C_SDA_PORT, GPIO_I2C_SDA_PIN); delay_cycles(CPUCLK_FREQ / 20000);
}

static void power_on(void)
{
    bs_port_pin_write(BS_PIN_HDC_PWR, 0);     /* HDC2080 and DRV5055: low = on */
    bs_port_pin_write(BS_PIN_DRV_PWR, 0);
    bs_port_pin_write(BS_PIN_OPT_PWR, 1);     /* OPT3001 and TMP117: high = on */
    bs_port_pin_write(BS_PIN_TMP_PWR, 1);
}

void bs_board_init(void)
{
    SYSCFG_DL_init();
    power_on();                                /* the sensors must be powered while the bus is recovered */
    delay_cycles(CPUCLK_FREQ / 20);
    bus_recover();
    SYSCFG_DL_GPIO_init();
    SYSCFG_DL_I2C_init();
    power_on();                                /* the GPIO init put the switches back to off */
    SysTick->LOAD = CPUCLK_FREQ / 1000u - 1u;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
    delay_cycles(CPUCLK_FREQ / 20);
}
