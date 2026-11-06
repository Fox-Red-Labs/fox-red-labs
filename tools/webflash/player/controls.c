/*
 * No-code controls for the LP-MSPM0L1306 (the Digital Lab): a pin, a timer and a bus, set from the
 * page without a build. The page writes one table into the last 1 kB flash sector; this program re-reads it and
 * applies it, and reports back through a status block in RAM that the page reads over the debugger.
 *
 * Built on TI's i2c_controller_rw_multibyte_fifo_poll example for the LP-MSPM0L1306 (its SysConfig sets up I2C0 on
 * PA0 SDA and PA1 SCL at 400 kHz, and LED2 red on PA26); this file replaces the example's main source.
 *
 * Table at CTL_TABLE_ADDR, little-endian 32-bit words (21 words, 84 bytes):
 *   0 magic 0x4C544346 ("FCTL"), 1 seq, 2 pin (0 to 27 on port A, 0xFF none), 3 mode (0 read, 1 low, 2 high, 3 toggle,
 *   4 PWM), 4 on_ms, 5 off_ms, 6 i2c_addr (0 none, 0x100 scan, else a 7-bit address), 7 i2c_reg (0x100 none),
 *   8 i2c_n (0 to 8), 9 pwm_hz (100 to 20000), 10 pwm_duty (0 to 100 percent), 11 flags (bit 0: scan as well as the
 *   read in slot 0), 12 to 20: read slots 1 to 3 as (addr, reg, n up to 4).
 * Status block (struct ctl, address in the manifest, 21 words): 0 magic 0x4C545354 ("TSTL"), 1 seq applied, 2 pin level
 *   read, 3 toggles so far, 4 slot 0 result (0 none, 1 acknowledged, 2 no answer), 5 and 6 slot 0's 8 bytes, 7 to 10
 *   128-bit scan map, 11 CPU clock, 12 PWM period in timer ticks, 13 PWM compare value, 14 PWM clock divide (prescale + 1),
 *   15 to 20: slots 1 to 3 as (result, 4 bytes).
 * PWM: the pin is handed to a TIMG timer in edge-aligned PWM at 32 MHz / divide / period; 0 and 100 percent are held
 * as a low or a high pin (no timer). Pins with a timer output: PA3, PA6, PA12, PA13, PA26, PA27.
 */
#include "ti_msp_dl_config.h"

#define TABLE_MAGIC  (0x4C544346u)
#define STATUS_MAGIC (0x4C545354u)
#define CYCLES_PER_MS (CPUCLK_FREQ / 1000)
#define MIN_MS (5u)
#define MAX_MS (10000u)
#define NO_PIN (0xFFu)
#define I2C_SCAN (0x100u)
#define NO_REG (0x100u)
#define MAX_N (8u)
#define MAX_N_EXTRA (4u)
#define PWM_MIN_HZ (100u)
#define PWM_MAX_HZ (20000u)

typedef struct {
    uint32_t magic, seq, pin, mode, on_ms, off_ms, i2c_addr, i2c_reg, i2c_n, pwm_hz, pwm_duty, flags;
    uint32_t extra[3][3];
} table_t;

typedef struct {
    uint32_t magic, seq, level, toggles, i2c, bytes[2], scan[4], cpuclk, pwm_period, pwm_cc, pwm_div;
    uint32_t extra[3][2];
} status_t;

volatile status_t ctl = {STATUS_MAGIC, 0, 0, 0, 0, {0, 0}, {0, 0, 0, 0}, CPUCLK_FREQ, 0, 0, 0, {{0, 0}, {0, 0}, {0, 0}}};
static uint32_t delay_i2c;   /* errata I2C_ERR_13: cycles to wait after a transfer starts (TI's example) */

/* pin control index of PAn on the MSPM0L130x: PINCM(n + 1) (the device header, IOMUX_PINCMx_PF_GPIOA_DIOn) */
static uint32_t pincm_of(uint32_t pin) { return pin; }   /* IOMUX_PINCM1 is 0 */

static void i2c_wait_idle(void)
{
    while (!(DL_I2C_getControllerStatus(I2C_INST) & DL_I2C_CONTROLLER_STATUS_IDLE)) { }
}

/* one transfer; returns 1 when the address was acknowledged and no error was flagged */
static uint32_t i2c_go(uint32_t addr, DL_I2C_CONTROLLER_DIRECTION dir, uint32_t n)
{
    i2c_wait_idle();
    DL_I2C_startControllerTransfer(I2C_INST, addr, dir, n);
    delay_cycles(delay_i2c);
    while (DL_I2C_getControllerStatus(I2C_INST) & DL_I2C_CONTROLLER_STATUS_BUSY) { }
    uint32_t st = DL_I2C_getControllerStatus(I2C_INST);
    i2c_wait_idle();
    return (st & (DL_I2C_CONTROLLER_STATUS_ERROR | DL_I2C_CONTROLLER_STATUS_ADDR_ACK)) ? 0u : 1u;
}

static void i2c_drain(uint8_t *out, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        uint32_t guard = 0;
        while (DL_I2C_isControllerRXFIFOEmpty(I2C_INST) && ++guard < 200000u) { }
        out[i] = DL_I2C_isControllerRXFIFOEmpty(I2C_INST) ? 0xFFu : DL_I2C_receiveControllerData(I2C_INST);
    }
    DL_I2C_flushControllerRXFIFO(I2C_INST);
}

/* one register read in a slot: optional register byte, then n bytes; returns 1 when both transfers were acknowledged */
static uint32_t read_slot(uint32_t addr_in, uint32_t reg_in, uint32_t n_in, uint32_t nmax, uint8_t *b)
{
    uint32_t addr = addr_in & 0x7Fu, n = n_in > nmax ? nmax : n_in, ok = 1u;
    if (reg_in != NO_REG) {
        uint8_t r = (uint8_t) reg_in;
        DL_I2C_fillControllerTXFIFO(I2C_INST, &r, 1u);
        ok = i2c_go(addr, DL_I2C_CONTROLLER_DIRECTION_TX, 1u);
    }
    if (ok && n) {
        ok = i2c_go(addr, DL_I2C_CONTROLLER_DIRECTION_RX, n);
        i2c_drain(b, n);
    }
    return ok;
}

static void do_i2c(const volatile table_t *t)
{
    uint8_t b[MAX_N] = {0};
    ctl.i2c = 0; ctl.bytes[0] = ctl.bytes[1] = 0;
    ctl.scan[0] = ctl.scan[1] = ctl.scan[2] = ctl.scan[3] = 0;
    for (uint32_t k = 0; k < 3u; k++) ctl.extra[k][0] = ctl.extra[k][1] = 0;
    uint32_t scan = (t->i2c_addr == I2C_SCAN) || (t->flags & 1u);
    uint32_t any = scan || (t->i2c_addr != 0u && t->i2c_addr != I2C_SCAN);
    for (uint32_t k = 0; k < 3u; k++) any |= (t->extra[k][0] != 0u);
    if (!any) return;
    SYSCFG_DL_I2C_init();                       /* the pins may have been used as GPIO since */
    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_I2C_IOMUX_SDA, GPIO_I2C_IOMUX_SDA_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_I2C_IOMUX_SCL, GPIO_I2C_IOMUX_SCL_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableHiZ(GPIO_I2C_IOMUX_SDA);
    DL_GPIO_enableHiZ(GPIO_I2C_IOMUX_SCL);
    DL_I2C_flushControllerTXFIFO(I2C_INST);
    DL_I2C_flushControllerRXFIFO(I2C_INST);
    if (scan) {                                 /* a one-byte read at every address: ack means a part is there */
        for (uint32_t a = 0x08u; a <= 0x77u; a++) {
            if (i2c_go(a, DL_I2C_CONTROLLER_DIRECTION_RX, 1u)) ctl.scan[a >> 5] |= 1u << (a & 31u);
            i2c_drain(b, 1u);
        }
        ctl.i2c = 1u;
    }
    if (t->i2c_addr != 0u && t->i2c_addr != I2C_SCAN) {
        uint32_t ok = read_slot(t->i2c_addr, t->i2c_reg, t->i2c_n, MAX_N, b);
        ctl.bytes[0] = (uint32_t) b[0] | ((uint32_t) b[1] << 8) | ((uint32_t) b[2] << 16) | ((uint32_t) b[3] << 24);
        ctl.bytes[1] = (uint32_t) b[4] | ((uint32_t) b[5] << 8) | ((uint32_t) b[6] << 16) | ((uint32_t) b[7] << 24);
        ctl.i2c = ok ? 1u : 2u;
    }
    for (uint32_t k = 0; k < 3u; k++) {
        if (t->extra[k][0] == 0u) continue;
        uint8_t e[MAX_N_EXTRA] = {0};
        uint32_t ok = read_slot(t->extra[k][0], t->extra[k][1], t->extra[k][2], MAX_N_EXTRA, e);
        ctl.extra[k][0] = ok ? 1u : 2u;
        ctl.extra[k][1] = (uint32_t) e[0] | ((uint32_t) e[1] << 8) | ((uint32_t) e[2] << 16) | ((uint32_t) e[3] << 24);
    }
}

/* pins with a timer output on the MSPM0L130x (device header IOMUX_PINCMx_PF_TIMGy_CCPz) */
typedef struct { uint8_t pin, cc; GPTIMER_Regs *tim; uint32_t pf; } pwm_pin_t;
static const pwm_pin_t pwm_pins[] = {
    {26u, 0u, TIMG1, IOMUX_PINCM27_PF_TIMG1_CCP0},
    {27u, 1u, TIMG1, IOMUX_PINCM28_PF_TIMG1_CCP1},
    {13u, 1u, TIMG0, IOMUX_PINCM14_PF_TIMG0_CCP1},
    {12u, 0u, TIMG0, IOMUX_PINCM13_PF_TIMG0_CCP0},
    {3u,  0u, TIMG2, IOMUX_PINCM4_PF_TIMG2_CCP0},
    {6u,  1u, TIMG0, IOMUX_PINCM7_PF_TIMG0_CCP1},
};
static GPTIMER_Regs *pwm_active = 0;

static void pwm_stop(void)
{
    if (pwm_active) { DL_TimerG_reset(pwm_active); pwm_active = 0; }
    ctl.pwm_period = ctl.pwm_cc = ctl.pwm_div = 0;
}

static void apply_pwm(const volatile table_t *t)
{
    uint32_t hz = t->pwm_hz < PWM_MIN_HZ ? PWM_MIN_HZ : (t->pwm_hz > PWM_MAX_HZ ? PWM_MAX_HZ : t->pwm_hz);
    uint32_t duty = t->pwm_duty > 100u ? 100u : t->pwm_duty, m = 1u << t->pin;
    const pwm_pin_t *pp = 0;
    for (uint32_t i = 0; i < sizeof(pwm_pins) / sizeof(pwm_pins[0]); i++) if (pwm_pins[i].pin == t->pin) pp = &pwm_pins[i];
    DL_GPIO_initDigitalOutput(pincm_of(t->pin));
    DL_GPIO_enableOutput(GPIOA, m);
    if (!pp || duty == 0u || duty == 100u) {     /* no timer output on this pin, or a steady level */
        if (duty == 100u) DL_GPIO_setPins(GPIOA, m); else DL_GPIO_clearPins(GPIOA, m);
        return;
    }
    uint32_t div = (CPUCLK_FREQ / hz + 65534u) / 65535u;       /* smallest divide that fits 16 bits */
    if (div < 1u) div = 1u;
    if (div > 256u) div = 256u;
    uint32_t period = CPUCLK_FREQ / (div * hz);
    uint32_t cc = (period * (100u - duty)) / 100u;               /* edge-aligned, counting down: high while the count is above cc */
    DL_TimerG_ClockConfig clk = {.clockSel = DL_TIMER_CLOCK_BUSCLK, .divideRatio = DL_TIMER_CLOCK_DIVIDE_1, .prescale = (uint8_t)(div - 1u)};
    DL_TimerG_PWMConfig cfg = {.pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN, .period = period, .isTimerWithFourCC = false, .startTimer = DL_TIMER_STOP};
    DL_TimerG_reset(pp->tim);
    DL_TimerG_enablePower(pp->tim);
    delay_cycles(POWER_STARTUP_DELAY);
    DL_TimerG_setClockConfig(pp->tim, &clk);
    DL_TimerG_initPWMMode(pp->tim, &cfg);
    DL_TimerG_setCounterControl(pp->tim, DL_TIMER_CZC_CCCTL0_ZCOND, DL_TIMER_CAC_CCCTL0_ACOND, DL_TIMER_CLC_CCCTL0_LCOND);
    DL_TimerG_setCaptureCompareOutCtl(pp->tim, DL_TIMER_CC_OCTL_INIT_VAL_LOW, DL_TIMER_CC_OCTL_INV_OUT_DISABLED,
        DL_TIMER_CC_OCTL_SRC_FUNCVAL, pp->cc);
    DL_TimerG_setCaptCompUpdateMethod(pp->tim, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, pp->cc);
    DL_TimerG_setCaptureCompareValue(pp->tim, cc, pp->cc);
    DL_TimerG_enableClock(pp->tim);
    DL_TimerG_setCCPDirection(pp->tim, pp->cc ? DL_TIMER_CC1_OUTPUT : DL_TIMER_CC0_OUTPUT);
    DL_GPIO_initPeripheralOutputFunction(pincm_of(t->pin), pp->pf);
    DL_GPIO_enableOutput(GPIOA, m);
    DL_TimerG_startCounter(pp->tim);
    pwm_active = pp->tim;
    ctl.pwm_period = period; ctl.pwm_cc = cc; ctl.pwm_div = div;
}

static void apply_pin(const volatile table_t *t)
{
    pwm_stop();
    if (t->pin > 27u) return;
    uint32_t m = 1u << t->pin;
    if (t->mode == 4u) {
        apply_pwm(t);
    } else if (t->mode == 0u) {
        DL_GPIO_initDigitalInput(pincm_of(t->pin));
    } else {
        DL_GPIO_initDigitalOutput(pincm_of(t->pin));
        if (t->mode == 2u) DL_GPIO_setPins(GPIOA, m); else DL_GPIO_clearPins(GPIOA, m);
        DL_GPIO_enableOutput(GPIOA, m);
    }
    if (t->mode != 0u) IOMUX->SECCFG.PINCM[pincm_of(t->pin)] |= IOMUX_PINCM_INENA_ENABLE;   /* the page reads a driven pad back through GPIO DIN */
}

int main(void)
{
    SYSCFG_DL_init();
    DL_I2C_ClockConfig cc;
    DL_I2C_getClockConfig(I2C_INST, &cc);
    delay_i2c = (3u * (cc.divideRatio + 1u)) * (CPUCLK_FREQ / (cc.clockSel == DL_I2C_CLOCK_MFCLK ? 4000000u : 32000000u));
    uint32_t applied = 0xFFFFFFFFu;
    while (1) {
        const volatile table_t *t = (const volatile table_t *) CTL_TABLE_ADDR;
        if (t->magic != TABLE_MAGIC) { delay_cycles(10u * CYCLES_PER_MS); continue; }
        if (t->seq != applied) {
            applied = t->seq;
            ctl.toggles = 0;
            apply_pin(t);
            do_i2c(t);
            ctl.seq = applied;
        }
        if (t->pin <= 27u && t->mode == 3u) {
            uint32_t on = t->on_ms < MIN_MS ? MIN_MS : (t->on_ms > MAX_MS ? MAX_MS : t->on_ms);
            uint32_t off = t->off_ms < MIN_MS ? MIN_MS : (t->off_ms > MAX_MS ? MAX_MS : t->off_ms);
            DL_GPIO_setPins(GPIOA, 1u << t->pin); delay_cycles(on * CYCLES_PER_MS);
            DL_GPIO_clearPins(GPIOA, 1u << t->pin); delay_cycles(off * CYCLES_PER_MS);
            ctl.toggles += 2u;
        } else {
            if (t->pin <= 27u) ctl.level = (DL_GPIO_readPins(GPIOA, 1u << t->pin) != 0u) ? 1u : 0u;
            delay_cycles(10u * CYCLES_PER_MS);
        }
    }
}
