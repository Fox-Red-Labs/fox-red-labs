/* SimpleFOC on the LP-AM13E230 + DRV8376EVM: one unattended run, open-loop velocity then closed-loop velocity on the Hall sensor.
   The SimpleFOC library (third-party/simplefoc/arduino-foc) is used as shipped; this file is the "sketch".
   Nothing is printed: the run writes g_log in RAM and ends with the driver off; tools/run_simplefoc.py reads it once afterwards
   (a probe read halts the core, so the run must be over first).
   Safety, all in this file: bus 8 to 15 V, nFAULT, Hall edge seen within 1 s, phase current over TRIP_MA on 4 loops in a row,
   run time caps, driver off (DRVOFF high, nSLEEP low) on any exit. */
#include "Arduino.h"
#include "BLDCMotor.h"
#include "drivers/BLDCDriver6PWM.h"
#include "sensors/HallSensor.h"
extern "C" {
#include "device.h"
#include "ti_sdk_dl_config.h"
void frl_time_init(void);
}

#ifndef FRL_DRY
#define FRL_DRY 0   /* 1 = bench check with no motor supply: skips the bus, fault and Hall-edge checks and the sensor alignment; measures loop time only */
#endif
#define POLE_PAIRS   4
#define PHASE_R      0.80f     /* ohm, lab measurement 0.78 to 0.98 (motor_phase_r rows) */
#define TRIP_MA      1500
#define NE           1700      /* Hall edges kept per run */
#define NS           700       /* 20 ms samples per run */
#define NI           2800      /* 5 ms current windows per run */

enum { ST_NONE = 0, ST_DONE = 1, ST_NO_VM = 2, ST_FAULT_AT_WAKE = 3, ST_OVERCURRENT = 4, ST_NO_HALL = 5, ST_BUS = 6, ST_NFAULT = 7, ST_INIT_FAIL = 8, ST_ALIGN_FAIL = 9 };

struct RunLog {
    uint32_t n_edge, n_samp, n_cur, loops, loop_cyc_min, loop_cyc_max; uint64_t loop_cyc_sum;
    uint32_t status, t_start_us, hold_start_ms, hold_end_ms;
    uint32_t edge_us[NE]; uint8_t edge_st[NE];
    float vel[NS], tgt[NS], uq[NS];
    int16_t ipk_mA[NI];
};
struct Log { uint32_t magic, version, done, vm_mV; int32_t rest[3]; uint32_t cpu_hz; RunLog run[2]; };
volatile Log g_log;   /* the only output; read over the probe after done == 0xD0E5 */

static void drv_sleep(bool s) { if (s) DL_GPIO_clearPins(DRVA_PORT, DRVA_NSLEEP_PIN); else DL_GPIO_setPins(DRVA_PORT, DRVA_NSLEEP_PIN); }
static void drv_off(bool o)   { if (o) DL_GPIO_setPins(DRVB_PORT, DRVB_DRVOFF_PIN); else DL_GPIO_clearPins(DRVB_PORT, DRVB_DRVOFF_PIN); }
static bool nfault(void)      { return DL_GPIO_readPins(DRVB_PORT, DRVB_NFAULT_ALT_PIN) == 0U; }
static uint32_t hall_raw(void)
{
    return (digitalRead(0) ? 4U : 0U) | (digitalRead(1) ? 2U : 0U) | (digitalRead(2) ? 1U : 0U);
}
static uint32_t vm_mV(void)   /* EVM bus divider 143 k over 6.04 k */
{
    uint32_t c = DL_ADC_readResult(APP_ADC_RESULT_INST, DL_ADC_SOC_NUMBER1);
    return (uint32_t)(((uint64_t)c * 3300ULL * 14904ULL) / (4095ULL * 604ULL));
}
static void isen(int32_t out[3])
{
    out[0] = (int32_t)DL_ADC_readResult(ADC_B_RESULT_INST, DL_ADC_SOC_NUMBER0);   /* ISENA PA27 */
    out[1] = (int32_t)DL_ADC_readResult(ADC_C_RESULT_INST, DL_ADC_SOC_NUMBER0);   /* ISENB PA30 */
    out[2] = (int32_t)DL_ADC_readResult(ADC_C_RESULT_INST, DL_ADC_SOC_NUMBER1);   /* ISENC PA29 */
}
static inline uint32_t cyc(void) { return DWT->CYCCNT; }

static BLDCDriver6PWM driver(0, 0, 0, 0, 0, 0, 100);
static BLDCMotor motor(POLE_PAIRS, PHASE_R);
static HallSensor sensor(0, 1, 2, POLE_PAIRS);

static void safe_off(void) { motor.disable(); drv_off(true); drv_sleep(true); }

/* One run. mode 0 = velocity_openloop with a V/f ramp, mode 1 = closed-loop velocity on the Hall sensor.
   Profile: ramp 0 to f_hz electrical at 4 Hz/s, hold hold_s, ramp down in 2 s. */
static void run(int mode, float f_hz, float hold_s)
{
    volatile RunLog &L = g_log.run[mode];
    const uint32_t ramp_ms = (uint32_t)(f_hz / 4.0f * 1000.0f), hold_ms = (uint32_t)(hold_s * 1000.0f), down_ms = 2000U;
    const uint32_t total_ms = ramp_ms + hold_ms + down_ms;
    const float k_rad = 6.2831853f / POLE_PAIRS;      /* electrical Hz to mechanical rad/s */
    int32_t i3[3]; uint32_t over = 0, last_hall = hall_raw(), last_edge_ms = 0, next_samp = 0, next_cur = 0; int16_t win_pk = 0;
    L.status = ST_NONE; L.hold_start_ms = ramp_ms; L.hold_end_ms = ramp_ms + hold_ms; L.loop_cyc_min = 0xFFFFFFFFU;
    L.t_start_us = micros();
    uint32_t t0 = micros();
    for (;;) {
        uint32_t now_us = micros(), ms = (now_us - t0) / 1000U;
        if (ms >= total_ms) { L.status = ST_DONE; break; }
        float f = ms < ramp_ms ? f_hz * (float)ms / (float)ramp_ms : (ms < ramp_ms + hold_ms ? f_hz : f_hz * (float)(total_ms - ms) / (float)down_ms);
        float target = f * k_rad;
        /* Hall edge log (the ground truth for speed, independent of SimpleFOC's own estimate) */
        uint32_t h = hall_raw();
        if (h != last_hall) { if (L.n_edge < NE) { L.edge_us[L.n_edge] = now_us - t0; L.edge_st[L.n_edge] = (uint8_t)h; L.n_edge++; } last_hall = h; last_edge_ms = ms; }
        if (!FRL_DRY && mode == 1 && ms - last_edge_ms > 1000U && ms > 1500U && f > 1.0f) { L.status = ST_NO_HALL; break; }
        /* protection */
        isen(i3);
        int32_t m = 0; for (int p = 0; p < 3; p++) { int32_t d = (i3[p] - g_log.rest[p]) * 3300 / 4095; if (d < 0) d = -d; if (d > m) m = d; }   /* mA at the assumed 1 V/A */
        if (m > win_pk) win_pk = (int16_t)m;
        over = (m > TRIP_MA) ? over + 1U : 0U;
        if (over >= 4U) { L.status = ST_OVERCURRENT; break; }
        if (!FRL_DRY) {
            if (nfault()) { L.status = ST_NFAULT; break; }
            uint32_t vm = vm_mV(); if (vm < 8000U || vm > 15000U) { L.status = ST_BUS; break; }
        }
        /* SimpleFOC: the lines being measured */
        uint32_t c0 = cyc();
        if (mode == 0) motor.voltage_limit = 0.25f + 0.04f * f;   /* V/f: the open-loop voltage is the limit */
        motor.loopFOC();
        motor.move(target);
        uint32_t dc = cyc() - c0;
        L.loops++; L.loop_cyc_sum += dc; if (dc < L.loop_cyc_min) L.loop_cyc_min = dc; if (dc > L.loop_cyc_max) L.loop_cyc_max = dc;
        /* logs */
        if (ms >= next_samp && L.n_samp < NS) { L.vel[L.n_samp] = motor.shaft_velocity; L.tgt[L.n_samp] = target; L.uq[L.n_samp] = motor.voltage.q; L.n_samp++; next_samp = ms + 20U; }
        if (ms >= next_cur && L.n_cur < NI) { L.ipk_mA[L.n_cur++] = win_pk; win_pk = 0; next_cur = ms + 5U; }
    }
    motor.move(0); safe_off();
}

int main(void)
{
    Device_Init(); SYSCFG_DL_init(); frl_time_init();
    DL_ADC_powerUp(APP_ADC_INST); DL_ADC_powerUp(ADC_B_INST); DL_ADC_powerUp(ADC_C_INST); DEVICE_DELAY_US(5000);
    DL_SYSCTL_disableDACOUT();
    DL_MCPWM_enableTBCLK();
    g_log.magic = 0x534F4643U; g_log.version = 1; g_log.cpu_hz = 200000000U;
    drv_off(true); drv_sleep(true); DEVICE_DELAY_MS(2); drv_sleep(false); DEVICE_DELAY_MS(10);   /* wake with outputs disabled */
    DEVICE_DELAY_MS(20);
    g_log.vm_mV = vm_mV();
    if (!FRL_DRY && g_log.vm_mV < 8000U) { g_log.run[0].status = ST_NO_VM; drv_sleep(true); g_log.done = 0xD0E5U; for (;;) {} }
    if (!FRL_DRY && nfault()) { g_log.run[0].status = ST_FAULT_AT_WAKE; drv_sleep(true); g_log.done = 0xD0E5U; for (;;) {} }
    int64_t acc[3] = {0, 0, 0}; int32_t t3[3];
    for (int i = 0; i < 400; i++) { isen(t3); for (int p = 0; p < 3; p++) acc[p] += t3[p]; DEVICE_DELAY_US(150); }
    for (int p = 0; p < 3; p++) g_log.rest[p] = (int32_t)(acc[p] / 400);

    driver.voltage_power_supply = FRL_DRY ? 12.4f : (float)g_log.vm_mV / 1000.0f;
    driver.voltage_limit = 2.0f; driver.pwm_frequency = 20000; driver.enable_active_high = false;   /* DRVOFF low = on */
    if (!driver.init()) { g_log.run[0].status = ST_INIT_FAIL; safe_off(); g_log.done = 0xD0E5U; for (;;) {} }
    motor.linkDriver(&driver);
    motor.foc_modulation = FOCModulationType::SpaceVectorPWM;
    motor.torque_controller = TorqueControlType::voltage;

    /* Run 1: open-loop velocity, no sensor */
    motor.controller = MotionControlType::velocity_openloop;
    motor.voltage_limit = 0.5f; motor.velocity_limit = 60.0f;
    motor.init();                       /* enables the driver */
    run(0, 20.0f, FRL_DRY ? 1.0f : 8.0f);
    DEVICE_DELAY_MS(2000);
    drv_sleep(false); DEVICE_DELAY_MS(10);

    /* Run 2: closed-loop velocity on the Hall sensor */
    if (g_log.run[0].status == ST_DONE) {
        sensor.pullup = Pullup::USE_EXTERN; sensor.init();
        motor.linkSensor(&sensor);
        motor.controller = MotionControlType::velocity;
        motor.voltage_limit = 1.2f; motor.voltage_sensor_align = 0.5f; motor.velocity_limit = 60.0f;
        motor.PID_velocity.P = 0.03f; motor.PID_velocity.I = 0.3f; motor.PID_velocity.D = 0.0f; motor.PID_velocity.output_ramp = 3.0f;
        motor.LPF_velocity.Tf = 0.01f;
        motor.init();
        if (FRL_DRY) { motor.zero_electric_angle = 0.0f; motor.sensor_direction = Direction::CW; }   /* no alignment without a supply */
        if (!motor.initFOC()) { g_log.run[1].status = ST_ALIGN_FAIL; safe_off(); }
        else run(1, 20.0f, FRL_DRY ? 1.0f : 8.0f);
    }
    safe_off();
    g_log.done = 0xD0E5U;
    for (;;) {}
}
