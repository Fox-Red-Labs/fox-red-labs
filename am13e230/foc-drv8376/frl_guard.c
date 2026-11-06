/*
 * Bench guard for TI's normal (MotorVision) build. See frl_guard.h. Fox Red Labs, 2026-10-17; not part of TI's SDK.
 */
#include <math.h>
#include <string.h>
#include "main.h"
#include "system_config.h"
#include "hall_cal.h"
#include "mtr1_init.h"
#include "export.h"
#include "frl_guard.h"
#include "motorvision_uart_hal.h"
#include "export_package.h"
#include "foc.h"
#include "hal_hall_sen_interface.h"

extern HallCal_Data_t hallSensorData;

volatile FRL_Guard_t frlGuard = {
    .version = FRL_GUARD_VERSION, .state = FRL_IDLE, .reason = FRL_R_NONE, .heartbeat = 0U,
    .runMs = 0U, .capMs = 12000U, .hostPackets = 0U,
    .peak_A = 0.0f, .limit_A = FRL_GUARD_LIMIT_DEF_A, .speedMax_Hz = 40.0f,
    .obsMode = 0U, .onObs = 0U, .handover_Hz = 0.0f,
    .busMin_V = 10.0f, .busMax_V = 14.5f,   /* the 12 V 0.5 A adapter; a host selects the pack window explicitly */
    .angleAdj_deg = FRL_GUARD_HALL_ADJ_DEG,  /* revision H: the measured Hall offset, not 0 (frl_guard.h) */
    .capChan = {FRL_CAP_IA, FRL_CAP_IB, FRL_CAP_IC, FRL_CAP_VBUS}, .capDecim = 1U, .capTrig = FRL_TRIG_NOW,
    .capLen = FRL_CAP_N, .capPre = 0U, .capWinIdxA = 0xFFFFFFFFU, .capWinIdxB = 0xFFFFFFFFU,
    .dirRotor = 1U, .dirNow = 1.0f, .testRamp_Aps = 2.0f, .ilimCode = FRL_ILIM_CODE_OFF, .ilimTest_V = 1.70f,
    /* H5: 1.0 A (was 1.5): the sense is linear only to +/-1.25 A at 1 V/A, so the ceiling is 1.2 A (frl_guard.h). The
     * BLY171S label is 1.8 A rated; the DRV8376 is 4.5 A peak. Host-writable within FRL_GUARD_LIMIT_MIN_A..MAX_A. */
};

static bool     prevRun     = false;
static uint16_t drvPhase    = 0U;    /* H6: 0 idle, 1 nSLEEP low, 2 waking */
static uint32_t drvMs       = 0U;
static volatile uint32_t isrSinceTick = 0U; /* control ISRs since the background tick last ran */
static uint32_t lastCyc = 0U, fracCyc = 0U;
static uint32_t quietMs     = 0U;
static bool     quietReset  = false; /* MotorVision reset once for the current quiet spell */
static uint32_t lastPackets = 0U;
/* revision G (2026-10-19, from the firmware review):
 * G1 keep checking current after a stop, G2 host-alive on a changing heartbeat only, G3 cross-check the clock,
 * G4 refuse a start with the standstill currents off zero, G5 reject NaN and latch the limits for a run */
static volatile uint32_t stopWatch = 0U;     /* G1: control ISRs left to watch after a stop */
static volatile uint32_t isrTotal  = 0U;     /* G3: every control ISR */
static uint32_t clkIsr0 = 0U, clkMs = 0U;    /* G3: window start */
static uint16_t lastBeat = 0U;               /* G2 */
static uint32_t beatQuietMs = 0U;            /* G2 */
static float runLimit = FRL_GUARD_LIMIT_DEF_A, runBusMin = 10.0f, runBusMax = 14.5f, runSpeedMax = 40.0f; /* G5: latched at a start */
/* revision H3: the Hall tracking observer drives only in a run that started with hallPll 1 (latched in the tick) */
static volatile uint16_t pllOn = 0U, runHallPll = 0U;
static volatile uint16_t pllSeedArm = 0U;         /* H3b: seed the speed reference once at the observer's first speed */
static volatile float pllAngleOut = 0.0f;         /* the observer's angle, for the capture channel */
/* revision H4: the calibrated Hall table switch (latched at a run start) and the mode whose angleAdj default is loaded */
static uint16_t runHallTable = 0U, lastHallTable = 0U;
/* revision H5: ILIMIT mode, direction rule and hold test, latched at a run start */
static uint16_t runIlim = 1U, runDirRotor = 1U;
static volatile uint16_t testOn = 0U;           /* the latched test mode while a run is on, else 0 */
static float    testAngleRad = 0.0f;            /* latched */
static volatile float testTarget = 0.0f, testStep = 1.0e-4f, testRef = 0.0f, testVd = 0.0f;
static float    cmpApplied = -1.0f;
static void hall_table_check(void);

/* revision H: control ISR timing and the capture buffer */
volatile uint32_t frlIsrT0 = 0U;                  /* FRL_GUARD_ISR_ENTER() stamps it first thing in the control ISR */
static volatile uint32_t isrAcc = 0U;             /* mean ISR cycles x 256, a 1/256 IIR updated every ISR */
static volatile uint32_t isrMaxCyc = 0U;          /* longest ISR in cycles */
volatile int16_t frlCapBuf[FRL_CAP_CH][FRL_CAP_N]; /* 16 KB in RAM_S */
enum { CAPK_ZERO = 0, CAPK_FLOAT = 1, CAPK_HALL = 2, CAPK_HALLRAW = 3 };
static const volatile float *capSrc[FRL_CAP_CH];  /* set at arming, read in the ISR */
static uint8_t  capKind[FRL_CAP_CH];
static float    capInv[FRL_CAP_CH];               /* counts per unit */
static uint32_t capWr = 0U, capFill = 0U, capDecCnt = 0U, capDecL = 1U, capLenL = FRL_CAP_N, capPreL = 0U;
static uint16_t capTrigL = FRL_TRIG_NOW;
static bool     capPrevRun = false;
static uint32_t capWinAcc = 0U;
#define CAP_MASK (FRL_CAP_N - 1U)
#define BARRIER() __asm volatile("" ::: "memory")


/* -ffast-math (the SDK's Release flags) lets the compiler assume no NaN or infinity, so a comparison cannot catch them:
 * the suite saw a NaN speed reference come back as -40 Hz (2026-10-19). Test the bits instead. */
static inline bool fin(float x) { uint32_t u; memcpy(&u, &x, sizeof u); return (u & 0x7F800000U) != 0x7F800000U; }

static void driver_off_now(void)
{
    DL_GPIO_setPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT, MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN);
    HAL_disablePWM(&halFOCConfig_M1.pwmCfg, &motorVars_M1.halInterface->halFOCDataHandle->pwmData);
}

static void stop(uint16_t reason)
{
    motorVars_M1.inputCtrl.flagEnableRunAndIdentify = false;
    motorVars_M1.inputCtrl.flagStartHallCal         = false;
    motorVars_M1.inputCtrl.speedRef_Hz              = 0.0f;
    frlGuard.reason = reason;
    frlGuard.onObs  = 0U;
    pllOn           = 0U;
    testOn          = 0U;   /* H5: the controller's own stop takes over at once */
    frlGuard.state  = (reason == FRL_R_HOST_STOP || reason == FRL_R_CAL_DONE) ? FRL_IDLE : FRL_STOPPED;
    stopWatch       = prevRun ? FRL_GUARD_STOPWATCH_ISRS : 0U;   /* G1: after a real run only (not a refused start) */
    prevRun         = false;
}

/* H5: read PA4 (AIN10, ADC channel 18) once at boot through ADC2 SOC2, borrowed from the phase-voltage read with the
 * PWM's ADC trigger paused and interrupts off, then restored: the commissioning build's method on this board
 * (commissioning.c, auxiliary_pull_probe). Returns volts, or 0 if the ADC stayed busy. */
static float ilim_readback_V(void)
{
    MCPWM_Regs *pwm = (MCPWM_Regs *)halFOCConfig_M1.pwmCfg.pwmHandle;
    ADC_Regs *adc = ADC2;
    uint32_t sum = 0U, n = 0U;
    __disable_irq(); DL_MCPWM_disableADCTrigger(pwm, DL_MCPWM_SOC_A);
    uint32_t t0 = DWT->CYCCNT;
    while (DL_ADC_isBusy(adc) && (uint32_t)(DWT->CYCCNT - t0) < 20000U) {}
    if (!DL_ADC_isBusy(adc)) {
        uint32_t savedSoc = adc->ADCSOC2CTL, savedSeq = adc->ADCSEQ1CONFIG;
        DL_ADC_selectSOCChannel(adc, DL_ADC_SOC_NUMBER2, DL_ADC_CH_ADCIN18);
        DL_ADC_setupSequencer(adc, DL_ADC_SEQ_NUMBER1, 1472U, DL_ADC_TRIGGER_SOFTWARE, DL_ADC_SOC_NUMBER0);
        for (unsigned k = 0U; k < 32U; k++) {
            DL_ADC_clearInterruptStatus(adc, DL_ADC_INT_NUMBER1);
            DL_ADC_forceSequencer(adc, DL_ADC_SEQ_NUMBER1);
            t0 = DWT->CYCCNT;
            while (!DL_ADC_getRawInterruptStatus(adc, DL_ADC_INT_NUMBER1) && (uint32_t)(DWT->CYCCNT - t0) < 20000U) {}
            if (!DL_ADC_getRawInterruptStatus(adc, DL_ADC_INT_NUMBER1)) break;
            sum += DL_ADC_readResult(ADC2RESULT, DL_ADC_SOC_NUMBER2); n++;
        }
        adc->ADCSOC2CTL = savedSoc; adc->ADCSEQ1CONFIG = savedSeq;
        DL_ADC_clearInterruptStatus(adc, DL_ADC_INT_NUMBER1);
    }
    /* H5b (2026-10-20): ADC2 INT1 is the control interrupt, so each forced conversion above left it pending in the NVIC,
     * and clearing the ADC's flag does not clear that. At boot the HAL is not set up yet: on H5 the control ISR ran at
     * __enable_irq() and bus-faulted on a null handle (BFAR 0x0000000C), before the UART started. */
    NVIC_ClearPendingIRQ(motor1CtrlISR);
    DL_MCPWM_enableADCTrigger(pwm, DL_MCPWM_SOC_A); __enable_irq();
    return n ? ((float)sum / (float)n) * (3.3f / 4096.0f) : 0.0f;
}

static uint16_t ilimTestCode = FRL_ILIM_CODE_TEST;   /* H7: from ilimTest_V, set in the tick */

static uint16_t ilim_code(uint16_t mode)
{
    return (mode == 1U) ? FRL_ILIM_CODE_ON : (mode == 2U) ? ilimTestCode : FRL_ILIM_CODE_OFF;
}

static void ilim_apply(uint16_t mode)
{
    uint16_t code = ilim_code(mode);
    DL_CMPSSLITE_setDACValueLow(CMPSS2, code);
    frlGuard.ilimCode = code;
}

void frl_guard_boot(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; /* the cycle counter is the guard's clock */
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;
    lastCyc = DWT->CYCCNT;
    /* ILIMIT (H5): the "on" code, read back; left floating, the driver faults before enable, so it is always driven */
    DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM_PA4);
    DL_CMPSSLITE_setDACValueLow(CMPSS2, FRL_ILIM_CODE_ON);
    DL_SYSCTL_enableDACOUT(DL_SYSCTL_DACOUTEN_CMPSS2_DACL);
    frlGuard.ilimCode = FRL_ILIM_CODE_ON;   /* on until frl_guard_ilim_check reads it back (H5b) */
}

/* H5b (2026-10-20): the read-back moved here from frl_guard_boot. At boot, before the HAL is set up, it returned 0 V
 * (no conversion completed; H5 also bus-faulted there, see ilim_readback_V). Called once after the offset calibration,
 * where the commissioning build ran the same read with the control ISR live, and before the first guard tick, which
 * turns ILIMIT off (full scale) when ilimOk is 0. */
void frl_guard_ilim_check(void)
{
    float v = ilim_readback_V();
    frlGuard.ilim_V = v;
    frlGuard.ilimOk = (v >= FRL_ILIM_OK_MIN_V && v <= FRL_ILIM_OK_MAX_V) ? 1U : 0U;
    frlGuard.ilimMode = frlGuard.ilimOk ? 1U : 0U;
    ilim_apply(frlGuard.ilimMode);
}

static inline __attribute__((always_inline)) void guard_isr_body(void)
{
    isrTotal++;
    if (frlGuard.state != FRL_RUNNING && frlGuard.state != FRL_CALIBRATING) {
        isrSinceTick = 0U;
        if (stopWatch) {   /* G1: after a stop, until the firmware is idle with the currents near zero, or 2 s */
            stopWatch--;
            float m = 0.0f;
            for (int i = 0; i < 3; i++) { float a = fabsf(halFOCDataObj_M1.adcData.I_A.value[i]); if (a > m) m = a; }
            if (!fin(m) || m > runLimit) {   /* the limit latched at the run's start, not the host-writable one */
                driver_off_now();
                frlGuard.reason = FRL_R_STOP_OVERCURRENT; frlGuard.state = FRL_STOPPED; stopWatch = 0U;
            } else if (motorVars_M1.outVars.motorState == MOTOR_STOP_IDLE && m < FRL_GUARD_OFFSET_A) {
                stopWatch = 0U;
            }
        }
        return;
    }
    /* dead-man: the cap and the host watchdog live in the background loop; if it stops running (blocked transmit,
     * a hang), the ISR would keep the motor energized, so after FRL_GUARD_BG_ISRS without a tick the ISR stops it */
    if (++isrSinceTick > FRL_GUARD_BG_ISRS) {
        driver_off_now();
        stop(FRL_R_BG_STALL);
        return;
    }
    float m = 0.0f;
    for (int i = 0; i < 3; i++) {
        float a = fabsf(halFOCDataObj_M1.adcData.I_A.value[i]);
        if (a > m) m = a;
    }
    if (fin(m) && m > frlGuard.peak_A) frlGuard.peak_A = m;
    if (!fin(m) || m > runLimit) {   /* the latched limit; a NaN current trips */
        driver_off_now();
        stop(FRL_R_OVERCURRENT);
    }
}

/* Capture, revision H: one sample of each channel every capDecim control ISRs, into a ring of FRL_CAP_N. While armed the
 * ring fills continuously so the samples before the trigger are there; at the trigger the last capPre of them become
 * samples 0..capPre-1, and capture runs until capLen. It only reads values the controller has already computed and writes
 * the ring and the capture fields: it never changes control. Idle cost: one load and two compares. */
static inline __attribute__((always_inline)) int16_t cap_q(float v)
{   /* frl_guard.c builds without finite-math, so a NaN fails both compares and is stored as 32767 */
    if (!(v <= 32767.0f)) return 32767;
    if (v < -32768.0f) return -32768;
    return (int16_t)v;
}

static inline __attribute__((always_inline)) void cap_sample(void)
{
    uint16_t st = frlGuard.capState;
    if (st != FRL_CAP_ARMED && st != FRL_CAP_CAPTURING) return;
    bool run = motorVars_M1.inputCtrl.flagEnableRunAndIdentify;
    if (st == FRL_CAP_ARMED) {
        bool hit = (capTrigL == FRL_TRIG_NOW) || (capTrigL == FRL_TRIG_RUN_STOP && capPrevRun && !run) ||
                   (capTrigL == FRL_TRIG_RUN_START && !capPrevRun && run);
        if (hit) {
            uint32_t pre = capFill < capPreL ? capFill : capPreL;
            frlGuard.capStart = (capWr - pre) & CAP_MASK;
            capFill = pre; frlGuard.capCount = pre;
            capDecCnt = capDecL - 1U;           /* the trigger ISR's own sample is sample capPre */
            frlGuard.capState = st = FRL_CAP_CAPTURING;
        }
    }
    capPrevRun = run;
    if (++capDecCnt < capDecL) return;
    capDecCnt = 0U;
    for (uint32_t c = 0U; c < FRL_CAP_CH; c++) {
        float v = 0.0f;
        switch (capKind[c]) {
        case CAPK_FLOAT: v = *capSrc[c]; break;
        case CAPK_HALL:  v = (float)hallData_M1.hallState; break;
        case CAPK_HALLRAW:
            v = (float)((DL_GPIO_readPins(MTR1_HALL_A_PORT, MTR1_HALL_A_PIN) ? 1U : 0U) |
                        (DL_GPIO_readPins(MTR1_HALL_B_PORT, MTR1_HALL_B_PIN) ? 2U : 0U) |
                        (DL_GPIO_readPins(MTR1_HALL_C_PORT, MTR1_HALL_C_PIN) ? 4U : 0U));
            break;
        default: break;
        }
        frlCapBuf[c][capWr] = cap_q(v * capInv[c]);
    }
    capWr = (capWr + 1U) & CAP_MASK;
    if (st == FRL_CAP_ARMED) { if (capFill < FRL_CAP_N) capFill++; return; }
    frlGuard.capCount = ++capFill;
    if (capFill >= capLenL) frlGuard.capState = FRL_CAP_DONE;
}

MOTOR_CTRL_RAMFUNC void frl_guard_isr(void)
{
    guard_isr_body();
    cap_sample();
    /* ISR timing: from FRL_GUARD_ISR_ENTER() to here; scaled to microseconds in the 1 ms tick, never divided here */
    uint32_t d = DWT->CYCCNT - frlIsrT0;
    uint32_t acc = isrAcc;
    isrAcc = acc ? acc + d - (acc >> 8) : d << 8;
    if (d > isrMaxCyc) isrMaxCyc = d;
}

static uint32_t elapsed_ms(void)
{   /* true milliseconds from the cycle counter: TI's background tick (LOW_PRIORITY_WAIT_TIME_1ms, 0.4 x PWM kHz
     * ISRs) is not 1 ms, and this guard's cap and host watchdog ran about 1.6 times fast on it (2026-10-17) */
    uint32_t now = DWT->CYCCNT, ms = 0U;
    fracCyc += now - lastCyc; lastCyc = now;
    while (fracCyc >= FRL_GUARD_CYC_PER_MS) { fracCyc -= FRL_GUARD_CYC_PER_MS; ms++; }
    return ms;
}

/* revision H: ISR timing in microseconds and as a share of the control period, from the ISR's cycle counts */
static void timing_tick(void)
{
    float avgCyc = (float)isrAcc * (1.0f / 256.0f);
    frlGuard.isrUs_avg   = avgCyc * (1000.0f / (float)FRL_GUARD_CYC_PER_MS);
    frlGuard.cpuLoad_pct = avgCyc * (100.0f / (float)FRL_GUARD_PERIOD_CYC);
    /* a reset is a host write of 0 over a value this tick published; testing the field alone read its own boot value 0 as a
     * reset every millisecond, so the max stayed 0 (guard suite, 2026-10-19) */
    static float pub = 0.0f;
    if (pub > 0.0f && !(frlGuard.isrUs_max > 0.0f)) isrMaxCyc = 0U;
    frlGuard.isrUs_max   = (float)isrMaxCyc * (1000.0f / (float)FRL_GUARD_CYC_PER_MS);
    pub = frlGuard.isrUs_max;
}

/* revision H: arming, cancelling and the readout window of the capture buffer */
static void cap_tick(uint32_t dms)
{
    static const float unit[FRL_CAP_CHANNELS] = {   /* units per count: ranges +/-8.19 A, +/-32.8 V, +/-6.55 rad, +/-327 Hz */
        1.0f, 0.00025f, 0.00025f, 0.00025f, 0.001f, 0.001f, 0.001f, 0.001f, 1.0f, 0.0002f,
        0.00025f, 0.00025f, 0.00025f, 0.00025f, 0.01f, 1.0f, 0.01f, 0.0002f };
    uint16_t arm = frlGuard.capArm;
    if (arm == 2U) {   /* cancel: a trigger that never comes (a run stop that never happens) must not hold the buffer */
        frlGuard.capState = FRL_CAP_IDLE;
    } else if (arm == 1U && (frlGuard.capState == FRL_CAP_IDLE || frlGuard.capState == FRL_CAP_DONE) &&
               frlGuard.state != FRL_CALIBRATING) {
        frlGuard.capState = FRL_CAP_IDLE;   /* the ISR ignores the buffer while it is set up */
        BARRIER();
        for (uint32_t c = 0U; c < FRL_CAP_CH; c++) {
            uint16_t ch = frlGuard.capChan[c];
            if (ch >= FRL_CAP_CHANNELS) ch = FRL_CAP_NONE;
            frlGuard.capChan[c] = ch;
            const volatile float *p = 0;
            switch (ch) {
            case FRL_CAP_IA: p = &halFOCDataObj_M1.adcData.I_A.value[0]; break;
            case FRL_CAP_IB: p = &halFOCDataObj_M1.adcData.I_A.value[1]; break;
            case FRL_CAP_IC: p = &halFOCDataObj_M1.adcData.I_A.value[2]; break;
            case FRL_CAP_VA: p = &halFOCDataObj_M1.adcData.V_V.value[0]; break;
            case FRL_CAP_VB: p = &halFOCDataObj_M1.adcData.V_V.value[1]; break;
            case FRL_CAP_VC: p = &halFOCDataObj_M1.adcData.V_V.value[2]; break;
            case FRL_CAP_VBUS: p = &halFOCDataObj_M1.adcData.VdcBus_V; break;
            case FRL_CAP_ANGLE: p = &motorVars_M1.outVars.angleFOC_rad; break;
            case FRL_CAP_ID: p = &motorVars_M1.outVars.Idq_in_A.value[0]; break;
            case FRL_CAP_IQ: p = &motorVars_M1.outVars.Idq_in_A.value[1]; break;
            case FRL_CAP_IDREF: p = &motorVars_M1.outVars.IdqRef_A.value[0]; break;
            case FRL_CAP_IQREF: p = &motorVars_M1.outVars.IdqRef_A.value[1]; break;
            case FRL_CAP_SPEED: p = &motorVars_M1.outVars.speed_Hz; break;
            case FRL_CAP_PLLSPEED: p = &frlGuard.pllSpeed_Hz; break;
            case FRL_CAP_PLLANGLE: p = &pllAngleOut; break;
            default: break;
            }
            capSrc[c]  = p;
            capKind[c] = p ? CAPK_FLOAT : ch == FRL_CAP_HALL ? CAPK_HALL : ch == FRL_CAP_HALLRAW ? CAPK_HALLRAW : CAPK_ZERO;
            frlGuard.capScale[c] = unit[ch];
            capInv[c] = 1.0f / unit[ch];
        }
        if (frlGuard.capDecim < 1U) frlGuard.capDecim = 1U;
        if (frlGuard.capDecim > FRL_CAP_DECIM_MAX) frlGuard.capDecim = FRL_CAP_DECIM_MAX;
        if (frlGuard.capLen < 16U) frlGuard.capLen = 16U;
        if (frlGuard.capLen > FRL_CAP_N) frlGuard.capLen = FRL_CAP_N;
        if (frlGuard.capTrig > FRL_TRIG_RUN_START) frlGuard.capTrig = FRL_TRIG_NOW;
        if (frlGuard.capTrig == FRL_TRIG_NOW) frlGuard.capPre = 0U;   /* nothing was recorded before "now" */
        if (frlGuard.capPre >= frlGuard.capLen) frlGuard.capPre = frlGuard.capLen - 1U;
        capDecL = frlGuard.capDecim; capLenL = frlGuard.capLen; capPreL = frlGuard.capPre; capTrigL = frlGuard.capTrig;
        capWr = 0U; capFill = 0U; capDecCnt = capDecL - 1U;
        capPrevRun = motorVars_M1.inputCtrl.flagEnableRunAndIdentify;
        frlGuard.capCount = 0U; frlGuard.capStart = 0U; frlGuard.capIndex = 0U;
        BARRIER();
        frlGuard.capState = FRL_CAP_ARMED;
    }
    frlGuard.capArm = 0U;   /* acted on, or refused: a refusal leaves capState as it was */

    /* the window: sample capIndex of each channel in units, between two sequence marks */
    if (frlGuard.capState != FRL_CAP_DONE || frlGuard.capCount == 0U) {
        frlGuard.capWinIdxA = 0xFFFFFFFFU; frlGuard.capWinIdxB = 0xFFFFFFFFU; capWinAcc = 0U;
        return;
    }
    uint32_t n = frlGuard.capCount, i = frlGuard.capIndex;
    if (frlGuard.capWinMs != 0U) {
        capWinAcc += dms;
        if (capWinAcc >= frlGuard.capWinMs) { capWinAcc = 0U; i = i + 1U; }
    }
    if (i >= n) i = 0U;
    frlGuard.capIndex = i;
    uint32_t at = (frlGuard.capStart + i) & CAP_MASK;
    frlGuard.capWinIdxA = i;
    BARRIER();
    for (uint32_t c = 0U; c < FRL_CAP_CH; c++) frlGuard.capWin[c] = (float)frlCapBuf[c][at] * frlGuard.capScale[c];
    BARRIER();
    frlGuard.capWinIdxB = i;
}

void frl_guard_tick_1ms(void)
{
    isrSinceTick = 0U;
    MV_Uart_Service(); /* a stuck receive FIFO would look like a lost host */
    uint32_t dms = elapsed_ms();
    uint32_t packets = EXPORT_getRxBufferIndex();
    if (packets != lastPackets) { lastPackets = packets; quietMs = 0U; quietReset = false; }
    else if (quietMs < 0xF0000000U) quietMs += dms;
    if (quietMs > FRL_GUARD_HOST_MS && !quietReset) {
        /* A host that left without stopping its poll leaves the board streaming (about 19 KB/s measured) until the next
         * host fights through it to sync. After a second of silence, stop it once: STOP_POLLING (76) empties the poll
         * list and INITCMD (40) turns the scope stream off (INITCMD alone leaves the poll list running), so a board with
         * no host is quiet. */
        uint16_t none[13] = {0U};
        EXPORTPACKAGE_takeActionOnMessage(76U, none, false);
        EXPORTPACKAGE_takeActionOnMessage(40U, none, false);
        quietReset = true;
    }
    frlGuard.hostPackets = packets;
    /* G2: the host counts as alive only while its heartbeat value changes; resync probes and filler bytes do not */
    if (frlGuard.heartbeat != lastBeat) { lastBeat = frlGuard.heartbeat; beatQuietMs = 0U; }
    else if (beatQuietMs < 0xF0000000U) beatQuietMs += dms;
    /* G3: the cap and the watchdog run on the cycle counter; check it against the control ISR count every second */
    clkMs += dms;
    uint32_t isrsWin = isrTotal - clkIsr0;
    if (isrsWin >= FRL_GUARD_ISR_HZ || clkMs >= 2000U) {   /* a frozen cycle counter closes on the ISR count, stalled ISRs on time */
        float ms = (float)clkMs, wantMs = (float)isrsWin * 1000.0f / (float)FRL_GUARD_ISR_HZ;
        bool bad = ms < (1.0f - FRL_GUARD_CLOCK_TOL) * wantMs || ms > (1.0f + FRL_GUARD_CLOCK_TOL) * wantMs;
        clkIsr0 = isrTotal; clkMs = 0U;
        if (bad && (frlGuard.state == FRL_RUNNING || frlGuard.state == FRL_CALIBRATING)) { driver_off_now(); stop(FRL_R_CLOCK); return; }
    }

    /* signals for the console's lights: raw Hall levels and edges, and the driver's nFAULT pin */
    uint16_t hall = (DL_GPIO_readPins(MTR1_HALL_A_PORT, MTR1_HALL_A_PIN) ? 1U : 0U) |
                    (DL_GPIO_readPins(MTR1_HALL_B_PORT, MTR1_HALL_B_PIN) ? 2U : 0U) |
                    (DL_GPIO_readPins(MTR1_HALL_C_PORT, MTR1_HALL_C_PIN) ? 4U : 0U);
    uint16_t moved = hall ^ frlGuard.hall;
    if (moved & 1U) frlGuard.hallEdgesA++;
    if (moved & 2U) frlGuard.hallEdgesB++;
    if (moved & 4U) frlGuard.hallEdgesC++;
    frlGuard.hall   = hall;
    frlGuard.nFault = DL_GPIO_readPins(MTR1_GATE_DRIVER_nFAULT_GPIO_PORT, MTR1_GATE_DRIVER_nFAULT_GPIO_PIN) ? 1U : 0U;

    timing_tick();
    cap_tick(dms);

    if (frlGuard.capMs > FRL_GUARD_CAP_MS_MAX) frlGuard.capMs = FRL_GUARD_CAP_MS_MAX;
    if (!fin(frlGuard.limit_A) || frlGuard.limit_A < FRL_GUARD_LIMIT_MIN_A) frlGuard.limit_A = FRL_GUARD_LIMIT_MIN_A;
    if (frlGuard.limit_A > FRL_GUARD_LIMIT_MAX_A) frlGuard.limit_A = FRL_GUARD_LIMIT_MAX_A;
    if (!fin(frlGuard.speedMax_Hz) || frlGuard.speedMax_Hz < FRL_GUARD_SPEED_MIN_HZ) frlGuard.speedMax_Hz = FRL_GUARD_SPEED_MIN_HZ;
    if (frlGuard.speedMax_Hz > FRL_GUARD_SPEED_MAX_HZ) frlGuard.speedMax_Hz = FRL_GUARD_SPEED_MAX_HZ;
    if (frlGuard.obsMode > 2U) frlGuard.obsMode = 0U;
    if (frlGuard.hallFree > 1U) frlGuard.hallFree = 0U;
    if (frlGuard.hallPll > 1U) frlGuard.hallPll = 0U;
    if (frlGuard.hallTable > 1U) frlGuard.hallTable = 0U;
    /* H5 */
    if (frlGuard.ilimMode > 2U || (frlGuard.ilimMode != 0U && !frlGuard.ilimOk)) frlGuard.ilimMode = 0U;
    if (frlGuard.dirRotor > 1U) frlGuard.dirRotor = 1U;
    if (frlGuard.testMode > 2U) frlGuard.testMode = 0U;
    if (!fin(frlGuard.testAngle_deg) || frlGuard.testAngle_deg < -360.0f || frlGuard.testAngle_deg > 360.0f) frlGuard.testAngle_deg = 0.0f;
    if (!fin(frlGuard.testId_A)) frlGuard.testId_A = 0.0f;
    if (frlGuard.testId_A >  FRL_GUARD_TEST_I_MAX_A) frlGuard.testId_A =  FRL_GUARD_TEST_I_MAX_A;
    if (frlGuard.testId_A < -FRL_GUARD_TEST_I_MAX_A) frlGuard.testId_A = -FRL_GUARD_TEST_I_MAX_A;
    if (!fin(frlGuard.testRamp_Aps) || frlGuard.testRamp_Aps < 0.0f) frlGuard.testRamp_Aps = 2.0f;
    if (frlGuard.testRamp_Aps > FRL_GUARD_TEST_RAMP_MAX) frlGuard.testRamp_Aps = FRL_GUARD_TEST_RAMP_MAX;
    if (!fin(frlGuard.testVd_V)) frlGuard.testVd_V = 0.0f;
    if (frlGuard.testVd_V >  FRL_GUARD_TEST_V_MAX_V) frlGuard.testVd_V =  FRL_GUARD_TEST_V_MAX_V;
    if (frlGuard.testVd_V < -FRL_GUARD_TEST_V_MAX_V) frlGuard.testVd_V = -FRL_GUARD_TEST_V_MAX_V;
    /* the live hold values: the ISR reads them; a ramp of 0 is a step */
    testTarget = frlGuard.testId_A;
    testStep   = (frlGuard.testRamp_Aps > 0.0f) ? frlGuard.testRamp_Aps * (1.0f / (float)FRL_GUARD_ISR_HZ) : 2.0f * FRL_GUARD_TEST_I_MAX_A;
    testVd     = frlGuard.testVd_V;
    frlGuard.testIdNow_A = testRef;
    {   /* the comparator trip: the host can write motorSetVars_M1.overCurrent_A; clamp it, apply a change at idle only */
        float c = motorSetVars_M1.overCurrent_A;
        if (!fin(c) || c < FRL_GUARD_CMP_MIN_A) c = FRL_GUARD_CMP_MIN_A;
        if (c > FRL_GUARD_CMP_MAX_A) c = FRL_GUARD_CMP_MAX_A;
        motorSetVars_M1.overCurrent_A = c;
        if (frlGuard.state != FRL_RUNNING && frlGuard.state != FRL_CALIBRATING && c != cmpApplied &&
            motorVars_M1.outVars.motorState == MOTOR_STOP_IDLE && cmpApplied >= 0.0f) {
            HAL_3S_FOC_SetCompOCLimits(&halFOCConfig_M1, &halFOCDataObj_M1);
            cmpApplied = c;
        }
        if (cmpApplied < 0.0f) cmpApplied = c;   /* the HAL applied the boot value after its offset calibration */
        frlGuard.cmpLimit_A = cmpApplied;
    }
    if (!fin(frlGuard.busMin_V) || frlGuard.busMin_V < FRL_GUARD_BUS_ABS_MIN_V) frlGuard.busMin_V = FRL_GUARD_BUS_ABS_MIN_V;
    if (!fin(frlGuard.busMax_V) || frlGuard.busMax_V > FRL_GUARD_BUS_ABS_MAX_V) frlGuard.busMax_V = FRL_GUARD_BUS_ABS_MAX_V;
    if (frlGuard.busMax_V < frlGuard.busMin_V + 1.0f) frlGuard.busMax_V = frlGuard.busMin_V + 1.0f;
    if (frlGuard.hallFree && frlGuard.obsMode == 0U) frlGuard.hallFree = 0U;   /* Hall-free needs an observer */
    /* TI's restart takes STARTUP_HALL (torque on the raw Hall angle for the first revolutions) when flying start is
     * off; Hall-free must start through align and open loop instead. Written from C, never over MotorVision (the
     * enum's width is unconfirmed). Only while idle, so a run never changes start mode under it. */
    if (!motorVars_M1.inputCtrl.flagEnableRunAndIdentify)
        motorVars_M1.inputCtrl.startupMode = frlGuard.hallFree ? STARTUP_ALIGN : STARTUP_HALL;
    /* H4: each Hall table has its own offset default; writing hallTable at idle loads it (write angleAdj_deg after) */
    float adjDef = frlGuard.hallTable ? FRL_GUARD_HALL_ADJ_CAL_DEG : FRL_GUARD_HALL_ADJ_DEG;
    if (frlGuard.state != FRL_RUNNING && frlGuard.state != FRL_CALIBRATING && frlGuard.hallTable != lastHallTable) {
        lastHallTable = frlGuard.hallTable;
        frlGuard.angleAdj_deg = adjDef;
    }
    if (!fin(frlGuard.angleAdj_deg) || frlGuard.angleAdj_deg < -90.0f || frlGuard.angleAdj_deg > 90.0f) frlGuard.angleAdj_deg = adjDef;
    if (!fin(frlGuard.obsTrimSet_deg) || frlGuard.obsTrimSet_deg < -180.0f || frlGuard.obsTrimSet_deg > 180.0f) frlGuard.obsTrimSet_deg = 0.0f;
    if (!fin(frlGuard.handover_Hz) || frlGuard.handover_Hz < 0.0f) frlGuard.handover_Hz = 0.0f;
    /* G5: limits written during a run go back to the values latched at its start; the speed reference stays live */
    if (frlGuard.state == FRL_RUNNING || frlGuard.state == FRL_CALIBRATING) {
        frlGuard.limit_A = runLimit; frlGuard.busMin_V = runBusMin; frlGuard.busMax_V = runBusMax; frlGuard.speedMax_Hz = runSpeedMax;
        frlGuard.hallPll = runHallPll;   /* H3: the observer switch is a run setting too */
        frlGuard.hallTable = runHallTable; /* H4: and the Hall table */
        frlGuard.ilimMode = runIlim; frlGuard.dirRotor = runDirRotor; frlGuard.testMode = testOn;   /* H5 */
    } else {
        /* H7: the test level from volts, at idle only */
        float lv = frlGuard.ilimTest_V;
        if (!fin(lv)) lv = 1.70f;
        if (lv < FRL_ILIM_TEST_MIN_V) lv = FRL_ILIM_TEST_MIN_V;
        if (lv > FRL_ILIM_TEST_MAX_V) lv = FRL_ILIM_TEST_MAX_V;
        frlGuard.ilimTest_V = lv;
        ilimTestCode = (uint16_t)((lv - FRL_ILIM_V_OFFSET) / FRL_ILIM_V_PER_CODE + 0.5f);
    }
    if (frlGuard.state != FRL_RUNNING && frlGuard.state != FRL_CALIBRATING && frlGuard.ilimCode != ilim_code(frlGuard.ilimMode)) {
        ilim_apply(frlGuard.ilimMode);   /* H5: a new ILIMIT level only at idle */
    }
    /* H6: driver sleep and wake on a host request, idle only (frl_guard.h, drvReset) */
    if (frlGuard.drvReset > 1U) frlGuard.drvReset = 0U;
    if (drvPhase == 0U) {
        if (frlGuard.drvReset == 1U && frlGuard.state != FRL_RUNNING && frlGuard.state != FRL_CALIBRATING && stopWatch == 0U &&
            !motorVars_M1.inputCtrl.flagEnableRunAndIdentify && !motorVars_M1.inputCtrl.flagStartHallCal) {
            DL_GPIO_setPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT, MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN);
            DL_GPIO_clearPins(MTR1_GATE_DRIVER_NSLEEP_GPIO_PORT, MTR1_GATE_DRIVER_NSLEEP_GPIO_PIN);
            drvPhase = 1U; drvMs = 0U;
        } else {
            frlGuard.drvReset = 0U;
        }
    } else {
        drvMs += dms;
        if (drvPhase == 1U && drvMs >= 2U) {
            DL_GPIO_setPins(MTR1_GATE_DRIVER_NSLEEP_GPIO_PORT, MTR1_GATE_DRIVER_NSLEEP_GPIO_PIN);
            drvPhase = 2U; drvMs = 0U;
        } else if (drvPhase == 2U && drvMs >= 7U) {
            /* H6b: DRVOFF stays high; the start preflight is the one place that lowers it (after a trip as well) */
            drvPhase = 0U; frlGuard.drvReset = 0U; frlGuard.drvResets++;
            /* H7: the sleep itself pulls nFAULT low, which the firmware records as a gate-driver fault and an estimator
             * error (bits 13 and 12). With nFAULT back high and no other bit set, the guard asks the firmware's own clear */
            if (frlGuard.nFault && (motorVars_M1.outVars.faultMtrNow.all & ~0x3000U) == 0U && motorVars_M1.outVars.faultMtrNow.all != 0U)
                motorVars_M1.inputCtrl.flagClearFaults = true;
        }
    }
    float sp = motorVars_M1.inputCtrl.speedRef_Hz;
    if (!fin(sp)) motorVars_M1.inputCtrl.speedRef_Hz = 0.0f;   /* G5: NaN or infinity goes to 0 */
    else if (sp > frlGuard.speedMax_Hz)  motorVars_M1.inputCtrl.speedRef_Hz = frlGuard.speedMax_Hz;
    else if (sp < -frlGuard.speedMax_Hz) motorVars_M1.inputCtrl.speedRef_Hz = -frlGuard.speedMax_Hz;

    bool  run = motorVars_M1.inputCtrl.flagEnableRunAndIdentify;
    bool  cal = motorVars_M1.inputCtrl.flagStartHallCal;
    float bus = halFOCDataObj_M1.adcData.VdcBus_V;

    if (run && !prevRun) { /* a host asked to start: preflight */
        uint16_t refuse = FRL_R_NONE;
        if (motorVars_M1.outVars.faultMtrNow.all != 0U || drvPhase != 0U) refuse = FRL_R_REFUSED_FAULT;   /* H6: not while the driver wakes */
        else if (bus < frlGuard.busMin_V || bus > frlGuard.busMax_V) refuse = FRL_R_REFUSED_BUS;
        else if (!cal && !hallSensorData.flagCalValid && !frlGuard.hallFree) refuse = FRL_R_REFUSED_UNCALIBRATED;
        else if (!cal && frlGuard.hallTable && !frlGuard.hallTableOk) refuse = FRL_R_REFUSED_UNCALIBRATED;   /* H4 */
        else {   /* G4: at standstill each phase and their sum must read near zero (offsets taken with the driver off read 1.5 A) */
            float a = halFOCDataObj_M1.adcData.I_A.value[0], b = halFOCDataObj_M1.adcData.I_A.value[1], d = halFOCDataObj_M1.adcData.I_A.value[2];
            if (!fin(a) || !fin(b) || !fin(d) || fabsf(a) >= FRL_GUARD_OFFSET_A || fabsf(b) >= FRL_GUARD_OFFSET_A ||
                fabsf(d) >= FRL_GUARD_OFFSET_A || fabsf(a + b + d) >= FRL_GUARD_OFFSET_A)
                refuse = FRL_R_REFUSED_OFFSET;
        }
        if (refuse != FRL_R_NONE) { stop(refuse); return; }
        DL_GPIO_clearPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT, MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN); /* after a trip */
        frlGuard.peak_A = 0.0f;
        runLimit = frlGuard.limit_A; runBusMin = frlGuard.busMin_V; runBusMax = frlGuard.busMax_V; runSpeedMax = frlGuard.speedMax_Hz; /* G5 */
        runHallPll = frlGuard.hallPll;
        runIlim = frlGuard.ilimMode; runDirRotor = frlGuard.dirRotor; frlGuard.dirFlips = 0U;   /* H5 */
        {   /* +/-360 degrees (clamped above) to +/-pi */
            float ta = frlGuard.testAngle_deg * 0.0174532925f;
            if (ta > 3.14159265f) ta -= 6.28318531f;
            if (ta > 3.14159265f) ta -= 6.28318531f;
            if (ta < -3.14159265f) ta += 6.28318531f;
            if (ta < -3.14159265f) ta += 6.28318531f;
            testAngleRad = ta;
        }
        testRef = 0.0f;
        testOn = cal ? 0U : frlGuard.testMode;   /* never during the Hall calibration */
        /* H4: the Hall table for this run. The firmware's Hall angle (HAL_Hall_computeAngle_inline), the standstill angle and
         * the tracking observer all read these two pointers; written only here, at idle, never during a run */
        runHallTable = frlGuard.hallTable;
        lastHallTable = frlGuard.hallTable;
        {   /* never during the calibration itself: it does not read them, and a new one replaces them */
            bool useCal = !cal && (runHallTable != 0U) && (frlGuard.hallTableOk != 0U);
            hallConfig_M1.calAngleFwdTablePtr = useCal ? hallSensorData.calAngleFwd_rad : NULL;
            hallConfig_M1.calAngleRevTablePtr = useCal ? hallSensorData.calAngleRev_rad : NULL;
        }
        pllOn = (runHallPll != 0U && !cal && !frlGuard.hallFree) ? 1U : 0U;   /* H3: never during the Hall calibration */
        pllSeedArm = pllOn;
        stopWatch = 0U; lastBeat = frlGuard.heartbeat; beatQuietMs = 0U;
        frlGuard.obsErrPk_deg = 0.0f;
        frlGuard.obsTrim_deg  = 0.0f;
        frlGuard.onObs  = 0U;
        frlGuard.runMs  = 0U;
        quietMs         = 0U;
        frlGuard.reason = FRL_R_NONE;
        frlGuard.state  = cal ? FRL_CALIBRATING : FRL_RUNNING;
        prevRun = true;
        return;
    }
    if (!run && prevRun) { stop(FRL_R_HOST_STOP); return; }
    if (!run) return;

    frlGuard.runMs += dms;
    uint32_t cap = (frlGuard.state == FRL_CALIBRATING) ? FRL_GUARD_CAL_CAP_MS : frlGuard.capMs;
    if (frlGuard.runMs >= cap) { stop(FRL_R_CAP); return; }
    if (beatQuietMs > FRL_GUARD_HOST_MS) { stop(FRL_R_HOST_LOST); return; }   /* G2 */
    if (motorVars_M1.outVars.faultMtrNow.all != 0U) { stop(FRL_R_TI_FAULT); return; }
    if (bus < frlGuard.busMin_V || bus > frlGuard.busMax_V) { stop(FRL_R_BUS); return; }
    if (frlGuard.state == FRL_CALIBRATING && hallSensorData.state == HALL_CAL_DONE && hallSensorData.flagCalDone) {
        /* H4: the SDK wires the calibrated tables into the Hall config in its low-priority MOTOR_HALL_CAL case, on the pass
         * after the averaging (hall_sen_foc_app.c). This stop comes first: it clears the run flag, the firmware's stop check
         * moves the motor to MOTOR_NORM_STOP before that case runs, and the tables were never wired (fwd_ptr_ok=0 in the
         * commissioning logs). The guard checks the tables here and wires them per run instead (frlGuard.hallTable). */
        hall_table_check();
        stop(FRL_R_CAL_DONE);
    }
}

/* ---- Shadow observer and hand-over (guard versions 4 and 5) -------------------------------------------------------------
 * TI's Hall solution builds and initialises its sensorless observers (ESMO, FAST) but never runs them
 * (hall_sen_foc_app.c leaves FOC_runObservers commented out). Here the selected observer runs every control ISR
 * beside the Halls, and its angle is compared with the Hall angle whichever one drives.
 * On this bench both observers lock to the Hall speed but read a steady offset from the Hall angle (ESMO 70 to 83
 * degrees with speed, FAST about 51 degrees, 2026-10-17), so the Halls train the observer: with handover_Hz set,
 * once TI is in control run at or above handover_Hz and the error has been steady (fast and slow filters within
 * FRL_OBS_STEADY_DEG) for FRL_OBS_ENGAGE_ISRS, the slow error is latched as obsTrim_deg and the FOC takes the
 * observer's angle minus the trim, and its speed. It hands back to the Halls below 0.8 x handover_Hz, outside
 * control run, or if the trimmed error passes FRL_OBS_DROP_DEG. The Halls start the motor in this mode.
 * Version 5: hallFree runs the observer minus a fixed trim from the first instant (TI's align and open-loop start, then
 * its open-loop-end merge onto angleObs_rad), the Halls logged as a referee only; angleAdj_deg shifts the FOC angle
 * in control run for the current-minimum sweep that shows where the true rotor angle is. */
#define FRL_OBS_STEADY_DEG   5.0f
#define FRL_OBS_DROP_DEG     45.0f
#define FRL_OBS_ENGAGE_ISRS  4000U   /* 200 ms at 20 kHz */
#define FRL_OBS_FAST_ALPHA   0.002f  /* about 25 ms at 20 kHz */
#define FRL_OBS_SLOW_ALPHA   0.0002f /* about 250 ms */
#define RAD2DEG              57.2957795f

static float    obsAngle = 0.0f, obsSpeed = 0.0f, errF = 0.0f, errS = 0.0f, trim = 0.0f;
static uint32_t okIsrs = 0U;

static inline float wrap_pi(float a)
{   /* G5: no loop, so an infinite or NaN angle cannot hang the ISR */
    if (!fin(a) || a < -1.0e4f || a > 1.0e4f) return 0.0f;
    return a - 6.28318531f * floorf((a + 3.14159265f) / 6.28318531f);
}

MOTOR_CTRL_RAMFUNC void frl_obs_pre(void *motor)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)motor;
    if (frlGuard.obsMode == 0U) return;
    obj->inputCtrl.estimatorMode = (frlGuard.obsMode == 2U) ? ESTIMATOR_MODE_FAST : ESTIMATOR_MODE_ESMO;
    FOC_runObservers(obj);
    obsAngle = obj->statusVars.angleObs_rad;
    obsSpeed = obj->outVars.speed_Hz;
}

/* revision H2: the Hall angle at standstill. The Hall code sets the angle from its table only on an edge and integrates the
 * speed between edges, so a start from rest used whatever angle the last run left behind: on 2026-10-19 every one of 12
 * Hall starts first turned the rotor one sector backwards, and 2 of 12 stalled at a balance point inside sector 2
 * (results/batch1-20261019). With no edge for FRL_HALL_STILL_ISRS the rotor is at rest somewhere inside the current
 * sector: take the middle of the forward and reverse table angles for that state (the two edges of the sector), plus the
 * mounting offset, so the first push is within 30 degrees of across the magnet whichever way the run goes. */
#define FRL_HALL_STILL_ISRS 2000U   /* 100 ms at 20 kHz: below 1.7 Hz electrical an edge is that far apart anyway */
static MOTOR_CTRL_RAMFUNC void hall_standstill_angle(MOTOR_Vars_t *obj)
{
    HAL_Hall_Data_t   *hd = (HAL_Hall_Data_t *)(obj->halSensorInterface->sensorData);
    HAL_Hall_Config_t *hc = (HAL_Hall_Config_t *)(obj->halSensorInterface->sensorConfig);
    uint16_t st = hd->hallState;
    if (hd->flagHallFault || st == 0U || st > 6U || hd->noEdgeIsrCnt < FRL_HALL_STILL_ISRS) return;
    const float32_t *f = (hc->calAngleFwdTablePtr != NULL) ? hc->calAngleFwdTablePtr : HAL_Hall_AngleTable_rad_fwd;
    const float32_t *r = (hc->calAngleRevTablePtr != NULL) ? hc->calAngleRevTablePtr : HAL_Hall_AngleTable_rad_rev;
    float a = wrap_pi(wrap_pi(f[st] + 0.5f * wrap_pi(r[st] - f[st])) + hc->angleOffset_rad);
    hd->angleHall_rad = a;
    obj->statusVars.angleHall_rad = a;
    obj->statusVars.angleObs_rad  = a;
}

/* revision H4: check the calibrated tables and publish them. HallCal_validate checks only that each step is 40 to 80
 * degrees in size, in either direction, and (H4) the sample counts; here the steps must also run forward in the forward
 * order 3, 2, 6, 4, 5, 1, and the two tables must agree on each boundary. Run once, when a calibration finishes. */
static const uint8_t calOrder[6] = { 3U, 2U, 6U, 4U, 5U, 1U };
static void hall_table_check(void)
{
    const float32_t *f = hallSensorData.calAngleFwd_rad, *r = hallSensorData.calAngleRev_rad;
    bool ok = hallSensorData.flagCalDone && hallSensorData.flagCalValid;
    float spread = 0.0f;
    for (uint32_t st = 1U; st <= 6U; st++) {
        frlGuard.calFwd_deg[st - 1U] = f[st] * RAD2DEG;
        frlGuard.calRev_deg[st - 1U] = r[st] * RAD2DEG;
        if (!fin(f[st]) || !fin(r[st])) ok = false;
        float a = hallSensorData.calDevFwdMax_rad[st] - hallSensorData.calDevFwdMin_rad[st];
        float b = hallSensorData.calDevRevMax_rad[st] - hallSensorData.calDevRevMin_rad[st];
        if (a > spread) spread = a;
        if (b > spread) spread = b;
    }
    for (uint32_t i = 0U; i < 6U && ok; i++) {
        uint8_t a = calOrder[i], b = calOrder[(i + 1U) % 6U];
        float sf = wrap_pi(f[b] - f[a]) * RAD2DEG, sr = wrap_pi(r[b] - r[a]) * RAD2DEG;
        float pair = fabsf(wrap_pi(r[a] - f[b])) * RAD2DEG;   /* backward into a and forward into b: the same boundary */
        if (sf < FRL_GUARD_CAL_STEP_MIN_DEG || sf > FRL_GUARD_CAL_STEP_MAX_DEG || sr < FRL_GUARD_CAL_STEP_MIN_DEG ||
            sr > FRL_GUARD_CAL_STEP_MAX_DEG || !(pair < FRL_GUARD_CAL_PAIR_DEG)) ok = false;
    }
    frlGuard.calSpread_deg = spread * RAD2DEG;
    frlGuard.hallTableOk = ok ? 1U : 0U;
}

/* ---- Hall tracking observer, revision H3 (guard version 8) ---------------------------------------------------------------
 * Batch 2 (results/batch2-20261019) measured the firmware's Hall speed with the wrong sign and size for tens of ms after a
 * start: its first estimate times an edge from the previous run and its sign is the commanded direction, not the edge
 * order, so after a one-edge back-step the speed loop drove reverse torque into a backward runaway. At steady speed its
 * per-edge estimate scatters about 20 percent (the bench's sectors are 48 to 69 degrees wide), which the speed loop turns
 * into torque noise. This observer tracks the electrical angle and a signed speed from the debounced edges:
 *   predict  every ISR theta += w x 2 pi dt. The travel since the last edge is bounded at FRL_PLL_STALE sectors: past it
 *            the rotor is slower than w, so |w| = that travel / the time since the edge and theta holds (the one division
 *            between edges, only while an edge is overdue).
 *   edge     the step from the forward order 3, 2, 6, 4, 5, 1 gives the direction; the edge angle is the calibrated
 *            table's (forward fwd[new], backward rev[new], as hall_standstill_angle reads them) plus the mounting offset,
 *            moved on by w x the debounce delay (2.5 ISRs).
 *              first edge after standstill: theta = edge, w = 0;
 *              second edge the same way: w = the angle between the two edges / the time between them;
 *              a reversal: w = 0 (never the old sign), theta = edge, and the second-edge rule again; if the next edge goes
 *                back within a quarter sector at the speed held before, it was a bounce and that speed comes back;
 *              tracking: e = wrap(edge - theta), theta += G1 e clamped to FRL_PLL_CMAX, w += G2 e / (2 pi T_edge).
 *            G1 = 1 - p^2, G2 = (1 - p)^2 put both poles of the per-edge error loop at p (FRL_PLL_POLE): it settles in
 *            the same number of edges at any speed. H3 used p = 0.6 (2 edges); H3b uses p = 0 (G1 = G2 = 1: the angle
 *            snaps to the edge and the speed is the edge-to-edge angle over the interval). Model: tools/motorlink/hall_pll_model.py.
 *   standstill  no edge for FRL_HALL_STILL_ISRS: theta = the sector middle, w = 0. Below about 1.7 Hz electrical (2 Hz
 *            with the bench's widest sector) every sector counts as standstill.
 * Per ISR, between edges: a multiply-add, a compare and the wraps, no division.
 * H3b (2026-10-19, after the H3 bench runs, results/guard-revH3-20261019/RESULTS.md): on the board the observer matched
 * the model sample by sample, but driving with it failed twice. (1) In the Hall start it held the angle at the edge it had
 * just crossed (speed 0 until its second edge), and the Hall start runs without the 60 degree offset, so the torque fell
 * to zero inside the sector: the suite's 20 Hz run never moved in 5 s, and the diagnostic run crept one edge per 100 ms
 * standstill reset. (2) In control run its filtering (pole 0.6, two edges of lag) let the speed loop at Kp 0.05 surge
 * between 6 and 35 Hz; the closed-loop model in tools/motorlink/hall_pll_model.py shows the same, and shows any lag in
 * the speed making the rotor's own speed swing more, not less, with these gains. So H3b: pole 0 (the angle snaps to each
 * edge and the speed is the last edge interval, as fast as the firmware's estimate, with the sign from the edge order
 * and the standstill, reversal, bounce and overdue-edge rules kept), the Hall start left to the firmware, and the speed
 * reference seeded once at the observer's first speed. The steady scatter that remains is not removed here; H4 wires the
 * calibrated table in per run (frlGuard.hallTable), and its pole-pair spread (calSpread_deg, 4 degrees on this motor)
 * says the Hall edges are regular, so the scatter is mostly speed ripple. */
#define FRL_PLL_POLE     0.0f   /* H3b: 0.6 in H3 (see the H3b note below) */
#define FRL_PLL_G1       (1.0f - FRL_PLL_POLE * FRL_PLL_POLE)
#define FRL_PLL_G2       ((1.0f - FRL_PLL_POLE) * (1.0f - FRL_PLL_POLE))
#define FRL_PLL_SECT     1.04719755f                       /* 60 degrees */
#define FRL_PLL_LIM      (1.2f * FRL_PLL_SECT)              /* FRL_PLL_STALE 1.2 sectors */
#define FRL_PLL_CMAX     FRL_PLL_SECT   /* H3b: 20 degrees in H3 */
#define FRL_PLL_BOUNCE   (0.25f * FRL_PLL_SECT)
#define FRL_PLL_DELAY    2.5f                               /* ISRs from a pin edge to the debounced state */
#define FRL_PLL_K        (6.28318531f / (float)FRL_GUARD_ISR_HZ)   /* rad per ISR at 1 Hz */
#define FRL_PLL_INVK     ((float)FRL_GUARD_ISR_HZ / 6.28318531f)   /* Hz from rad per ISR */
#define FRL_PI           3.14159265f

static const int8_t pllIdx[8] = { -1, 5, 1, 0, 3, 4, 2, -1 };   /* position in the forward order 3, 2, 6, 4, 5, 1 */
static float    pllTh = 0.0f, pllW = 0.0f, pllAdv = 0.0f, pllLastEa = 0.0f, pllWSave = 0.0f;
static uint32_t pllTE = 0xFFFFFFFFU;                           /* ISRs since the last edge */
static uint8_t  pllN = 0U, pllPend = 0U;
static int8_t   pllDir = 1, pllDirSave = 1;

static inline __attribute__((always_inline)) float wrap_s(float a)
{   /* for angles within +/-3 pi: two conditional steps, no floor or division */
    if (a >= FRL_PI) a -= 2.0f * FRL_PI; else if (a < -FRL_PI) a += 2.0f * FRL_PI;
    if (a >= FRL_PI) a -= 2.0f * FRL_PI; else if (a < -FRL_PI) a += 2.0f * FRL_PI;
    return a;
}

static MOTOR_CTRL_RAMFUNC void hall_pll(MOTOR_Vars_t *obj)
{
    HAL_Hall_Data_t   *hd = (HAL_Hall_Data_t *)(obj->halSensorInterface->sensorData);
    HAL_Hall_Config_t *hc = (HAL_Hall_Config_t *)(obj->halSensorInterface->sensorConfig);
    const float32_t *f = (hc->calAngleFwdTablePtr != NULL) ? hc->calAngleFwdTablePtr : HAL_Hall_AngleTable_rad_fwd;
    const float32_t *r = (hc->calAngleRevTablePtr != NULL) ? hc->calAngleRevTablePtr : HAL_Hall_AngleTable_rad_rev;
    float th = pllTh, w = pllW;
    if (pllTE != 0xFFFFFFFFU) pllTE++;
    float d = w * FRL_PLL_K;
    th += d; pllAdv += fabsf(d);
    if (pllAdv > FRL_PLL_LIM) {   /* an edge is overdue: hold the angle, and the rotor is at most this fast */
        float back = pllAdv - FRL_PLL_LIM;
        th -= (w > 0.0f) ? back : -back;
        pllAdv = FRL_PLL_LIM;
        float m = FRL_PLL_LIM * FRL_PLL_INVK / (float)pllTE;
        w = (w > 0.0f) ? m : -m;
    }
    th = wrap_s(th);
    uint16_t sn = hd->hallState, sp = hd->hallStatePrev;
    bool valid = !hd->flagHallFault && sn >= 1U && sn <= 6U;
    if (hd->flagNewEdge && valid && sp >= 1U && sp <= 6U) {
        int step = pllIdx[sn] - pllIdx[sp];
        if (step < 0) step += 6;
        if (step == 3) {   /* opposite sector: the direction is unknown; start again from the sector middle */
            pllN = 0U; pllPend = 0U;
            th = wrap_s(wrap_s(f[sn] + 0.5f * wrap_s(r[sn] - f[sn])) + hc->angleOffset_rad);
        } else {
            int8_t sgn = (step <= 2) ? 1 : -1;
            float ea = wrap_s(((sgn > 0) ? f[sn] : r[sn]) + hc->angleOffset_rad + w * (FRL_PLL_K * FRL_PLL_DELAY));
            float e  = wrap_s(ea - th);
            if (pllN == 0U) {                       /* first edge after standstill: direction and angle, no speed yet */
                th = ea; w = 0.0f; pllN = 1U; pllDir = sgn; pllPend = 0U;
            } else if (sgn != pllDir) {             /* a reversal */
                if (pllPend && sgn == pllDirSave && (float)pllTE * fabsf(pllWSave) * FRL_PLL_K < FRL_PLL_BOUNCE) {
                    w = pllWSave; th = ea; pllN = 2U; pllDir = sgn; pllPend = 0U;   /* back within a quarter sector: a bounce */
                } else {
                    pllWSave = w; pllDirSave = pllDir; pllPend = 1U;
                    th = ea; w = 0.0f; pllN = 1U; pllDir = sgn;
                }
            } else {
                pllPend = 0U;
                if (pllN == 1U) {                   /* second edge the same way: the speed from the two edges */
                    float span = wrap_s(ea - pllLastEa) * (float)sgn;
                    if (span < 0.5f * FRL_PLL_SECT) span = 0.5f * FRL_PLL_SECT;
                    if (span > 1.5f * FRL_PLL_SECT) span = 1.5f * FRL_PLL_SECT;
                    w = (float)sgn * span * FRL_PLL_INVK / (float)pllTE;
                    th = ea; pllN = 2U;
                } else {                            /* tracking */
                    float ec = (e > FRL_PLL_SECT) ? FRL_PLL_SECT : (e < -FRL_PLL_SECT) ? -FRL_PLL_SECT : e;
                    float c  = FRL_PLL_G1 * ec;
                    if (c > FRL_PLL_CMAX) c = FRL_PLL_CMAX; else if (c < -FRL_PLL_CMAX) c = -FRL_PLL_CMAX;
                    th = wrap_s(th + c);
                    w += FRL_PLL_G2 * ec * FRL_PLL_INVK / (float)pllTE;
                    frlGuard.pllErr_deg = e * RAD2DEG;
                }
            }
            pllLastEa = ea;
        }
        pllTE = 0U; pllAdv = 0.0f;
    }
    if (valid && pllTE >= FRL_HALL_STILL_ISRS) {   /* standstill, as hall_standstill_angle */
        th = wrap_s(wrap_s(f[sn] + 0.5f * wrap_s(r[sn] - f[sn])) + hc->angleOffset_rad);
        w = 0.0f; pllN = 0U; pllPend = 0U;
    }
    if (!fin(th) || !fin(w) || th > 4.0f || th < -4.0f) { th = 0.0f; w = 0.0f; pllN = 0U; pllPend = 0U; }
    pllTh = th; pllW = w;
    pllAngleOut = th; frlGuard.pllSpeed_Hz = w;

    /* hallPll 1: the observer drives in control run, unless Hall-free mode or the observer hand-over owns the angle
     * (onObs; the hand-over code below overrides these when it engages, and trains against this angle). H3b: the Hall
     * start is left to the firmware (see the note above). */
    if (pllOn && !frlGuard.hallFree) {
        MOTOR_Status_e ms = obj->outVars.motorState;
        if (ms == MOTOR_CTRL_RUN || ms == MOTOR_CL_RUNNING) {
            obj->statusVars.angleObs_rad = th; obj->outVars.speed_Hz = w;
            /* H3b: the firmware's Hall start hands over at the first edge and seeds the speed reference with its own
             * speed there, near 0 Hz, while the rotor reaches about 18 Hz by the second edge; the loop then brakes at full
             * current (batch 2, H2_06: IQREF -0.4 A at 24 ms), which is the back-step. Once, in the first second of a run,
             * at the observer's first speed with the reference's sign, the reference ramp restarts from that speed
             * (never past the target). */
            if (pllSeedArm) {
                float ref = obj->inputCtrl.speedRef_Hz;
                if (frlGuard.runMs > 1000U) pllSeedArm = 0U;
                else if (pllN >= 2U && w * ref > 0.0f) {
                    float sd = (fabsf(w) < fabsf(ref)) ? w : ref;
                    TRAJ_setIntValue(&obj->traj_spd, sd);
                    obj->statusVars.speed_int_Hz = sd;
                    pllSeedArm = 0U;
                }
            }
        }
    }
}

MOTOR_CTRL_RAMFUNC void frl_obs_post(void *motor)
{
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)motor;
    if (!frlGuard.hallFree) hall_standstill_angle(obj);
    hall_pll(obj);   /* revision H3: always runs; drives only with hallPll latched on */
    if (frlGuard.obsMode == 0U) {
        frlGuard.onObs = 0U; okIsrs = 0U;
        if ((obj->outVars.motorState == MOTOR_CTRL_RUN || obj->outVars.motorState == MOTOR_CL_RUNNING) && frlGuard.angleAdj_deg != 0.0f)
            obj->statusVars.angleObs_rad = wrap_pi(obj->statusVars.angleObs_rad + frlGuard.angleAdj_deg / RAD2DEG);
        return;
    }
    float hallAngle = obj->statusVars.angleObs_rad;   /* FOC_runHallObserver just wrote the Hall values */
    float hallSpeed = obj->outVars.speed_Hz;
    float e = wrap_pi(obsAngle - hallAngle);           /* raw offset, untrimmed */
    errF += FRL_OBS_FAST_ALPHA * (wrap_pi(e - errF));
    errS += FRL_OBS_SLOW_ALPHA * (wrap_pi(e - errS));
    errF = wrap_pi(errF); errS = wrap_pi(errS);
    bool  ctrlRun = (obj->outVars.motorState == MOTOR_CTRL_RUN || obj->outVars.motorState == MOTOR_CL_RUNNING);   /* H5: as hall_pll */
    float trimmed = wrap_pi(errF - (frlGuard.onObs ? trim : 0.0f)) * RAD2DEG;
    frlGuard.obsErr_deg  = trimmed;
    frlGuard.obsSpeed_Hz = obsSpeed;
    if (ctrlRun && fabsf(trimmed) > frlGuard.obsErrPk_deg && frlGuard.onObs) frlGuard.obsErrPk_deg = fabsf(trimmed);

    float adj = frlGuard.angleAdj_deg / RAD2DEG;
    if (frlGuard.hallFree) {   /* the observer drives from the start; TI's align and open-loop start need no Halls */
        trim = frlGuard.obsTrimSet_deg / RAD2DEG;
        frlGuard.onObs = 1U;
        frlGuard.obsErr_deg = wrap_pi(errF - trim) * RAD2DEG;   /* referee: trimmed observer minus Hall */
        if (ctrlRun && fabsf(frlGuard.obsErr_deg) > frlGuard.obsErrPk_deg) frlGuard.obsErrPk_deg = fabsf(frlGuard.obsErr_deg);
        obj->statusVars.angleObs_rad = wrap_pi(obsAngle - trim + (ctrlRun ? adj : 0.0f));
        obj->outVars.speed_Hz        = obsSpeed;
        return;
    }
    float h = frlGuard.handover_Hz;
    if (frlGuard.onObs == 0U) {
        bool steady = fabsf(wrap_pi(errF - errS)) * RAD2DEG < FRL_OBS_STEADY_DEG;
        okIsrs = (ctrlRun && h > 0.0f && fabsf(hallSpeed) >= h && steady) ? okIsrs + 1U : 0U;
        if (okIsrs >= FRL_OBS_ENGAGE_ISRS) {
            trim = errS; frlGuard.obsTrim_deg = trim * RAD2DEG; frlGuard.obsErrPk_deg = 0.0f; frlGuard.onObs = 1U;
        }
    } else if (!ctrlRun || h <= 0.0f || fabsf(obsSpeed) < 0.8f * h || fabsf(trimmed) > FRL_OBS_DROP_DEG) {
        frlGuard.onObs = 0U; okIsrs = 0U;
    }
    if (frlGuard.onObs) {   /* TI's closed-loop state takes angleObs_rad; its speed loop takes outVars.speed_Hz */
        obj->statusVars.angleObs_rad = wrap_pi(obsAngle - trim + (ctrlRun ? adj : 0.0f));
        obj->outVars.speed_Hz        = obsSpeed;
    } else if (ctrlRun && adj != 0.0f) {
        obj->statusVars.angleObs_rad = wrap_pi(hallAngle + adj);
    }
}

/* ---- Revision H5 (guard version 10): direction from the rotor, and the hold test mode ---------------------------------
 * Direction. The firmware sets inputCtrl.direction from the sign of the speed reference in its low-priority loop
 * (application.h), and the Hall speed's sign, the forward or reverse angle table, the angle integration between edges and
 * the capture's next expected line all follow it. A host that writes a target of 0 (a ramp stop) flipped it to reverse
 * at once while the rotor still turned forward, and a one-edge back-step at a start was read as forward speed (batch 2).
 * Here, while the Hall tracking observer (computed every ISR whichever source drives) reads more than FRL_DIR_HOLD_HZ,
 * the direction is the rotor's; below it, the reference's sign, and a zero reference keeps the last direction. */
float frl_direction(float speedRef_Hz, float prev)
{
    float d;
    if (!runDirRotor || frlGuard.state != FRL_RUNNING) {   /* idle and the calibration keep the firmware's rule */
        d = (speedRef_Hz > 0.0f) ? 1.0f : -1.0f;
        if (runDirRotor && speedRef_Hz == 0.0f) d = (prev < 0.0f) ? -1.0f : 1.0f;
    } else {
        float w = frlGuard.pllSpeed_Hz;
        if (!fin(w)) w = 0.0f;
        if (w > FRL_DIR_HOLD_HZ) d = 1.0f;
        else if (w < -FRL_DIR_HOLD_HZ) d = -1.0f;
        else if (speedRef_Hz > 0.0f) d = 1.0f;
        else if (speedRef_Hz < 0.0f) d = -1.0f;
        else d = (prev < 0.0f) ? -1.0f : 1.0f;
    }
    if (d != frlGuard.dirNow) { frlGuard.dirFlips++; frlGuard.dirNow = d; }
    return d;
}

/* Hold test. testMode latched at a run start: 1 holds Id (ramped to testId_A at testRamp_Aps) at testAngle_deg with Iq 0
 * through the firmware's own current loop; 2 sets Vd = testVd_V and Vq 0 with the current loop's output replaced (its
 * references follow the measured currents so its integrators do not wind up). The controller's start sequence runs
 * underneath; these three hooks override angle, reference and voltage after it. The guard's caps, trips and watchdogs
 * all stay in force. The rotor aligns to the d axis once (a jump of up to half an electrical turn at the first current),
 * then a d-axis hold makes no torque. */
MOTOR_CTRL_RAMFUNC void frl_test_angle(void *motor)
{
    if (!testOn) return;
    ((MOTOR_Vars_t *)motor)->outVars.angleFOC_rad = testAngleRad;
}

MOTOR_CTRL_RAMFUNC void frl_test_ref(void *motor)
{
    if (!testOn) return;
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)motor;
    if (testOn == 1U) {
        float e = testTarget - testRef, st = testStep;
        testRef += (e > st) ? st : (e < -st) ? -st : e;
        obj->outVars.Idq_out_A.value[0] = testRef;
        obj->outVars.Idq_out_A.value[1] = 0.0f;
    } else {
        obj->outVars.Idq_out_A.value[0] = obj->outVars.Idq_in_A.value[0];
        obj->outVars.Idq_out_A.value[1] = obj->outVars.Idq_in_A.value[1];
    }
}

MOTOR_CTRL_RAMFUNC void frl_test_volt(void *motor)
{
    if (testOn != 2U) return;
    MOTOR_Vars_t *obj = (MOTOR_Vars_t *)motor;
    obj->outVars.Vdq_out_V.value[0] = testVd;
    obj->outVars.Vdq_out_V.value[1] = 0.0f;
}
