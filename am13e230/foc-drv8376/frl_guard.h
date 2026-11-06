/*
 * Bench guard for TI's normal (MotorVision) build, Fox Red Labs, 2026-10-17. Not part of TI's SDK.
 *
 * TI's normal image runs the motor whenever a host writes flagEnableRunAndIdentify. On this bench the host is a web
 * page, so the limits the commissioning keys enforce (commissioning.c) live here instead:
 *   - ILIMIT (PA4, CMPSS2 low DAC): revision H5 sets the DRV8376 cycle-by-cycle limit to a 1.5 A sum after a read-back
 *     (FRL_ILIM_*); before H5 it was at full scale (off). The driver's fixed OCP, its thermal shutdown and the
 *     comparator trips stay on; H5 makes the comparators two-sided on phases U and W.
 *   - a start is refused while TI reports a fault or the bus is out of range;
 *   - a run stops at capMs (calibration at FRL_GUARD_CAL_CAP_MS), when the host goes quiet for FRL_GUARD_HOST_MS,
 *     when TI raises a fault, or when any phase current exceeds limit_A (the driver is switched off at once);
 *   - the speed reference is clamped to +/- speedMax_Hz;
 *   - if the background loop stops (FRL_GUARD_BG_ISRS control ISRs without a tick), the ISR turns the driver off.
 * Times come from the CPU cycle counter: TI's background tick is not a millisecond.
 * Its state is published through the MotorVision table (motorvision_config/motorvision_hash.c) as frlGuard.*.
 */
#ifndef FRL_GUARD_H
#define FRL_GUARD_H

#include <stdint.h>
#include <stdbool.h>

#define FRL_GUARD_VERSION      12U      /* bump on ANY change to the MotorVision table layout; a host must refuse writes
                                         * unless this equals the version its variable map was built for (frlGuard.version
                                         * stays at table index 5, right after TI's five uint16 entries). 1: 59 entries;
                                         * 2: 64 entries, Hall lines, edges and nFAULT added;
                                         * 3: 70 entries, the shadow observer and its hand-over added;
                                         * 4: 71 entries, the Hall-trained observer trim added;
                                         * 5: 74 entries, angle sweep, Hall-free mode and a fixed trim added;
                                         * 6: 76 entries, a host-settable bus window for the battery added;
                                         * 7: 103 entries, control ISR timing and the capture buffer added;
                                         * 8: 106 entries, the Hall tracking observer added (revision H3);
                                         * 9: 121 entries, the calibrated Hall table switch and its published angles
                                         *    added (revision H4);
                                         * 10: 135 entries, ILIMIT, direction and hold-test entries (revision H5);
                                         * 11: 139 entries, the driver sleep-and-wake request and the commanded
                                         *    d and q voltages added (revision H6);
                                         * 12: 140 entries, a host-settable ILIMIT test voltage added (revision H7). */
#define FRL_GUARD_CAP_MS_MAX   60000U
#define FRL_GUARD_CAL_CAP_MS   700000U /* TI's Hall calibration, as shipped, takes about 171 s here */
#define FRL_GUARD_HOST_MS      1000U
#define FRL_GUARD_BG_ISRS      4000U   /* 200 ms of control ISRs at 20 kHz without a background tick */
#define FRL_GUARD_CYC_PER_MS   200000U /* 200 MHz CPU */
#define FRL_GUARD_LIMIT_MIN_A  0.3f
#define FRL_GUARD_LIMIT_MAX_A  1.2f   /* H5: inside the current sense's linear range (+/-1.25 A at 1 V/A, VREF 3.0 V,
                                        * drv8376_interface.h), so a clipped reading still trips; was 1.8 */
#define FRL_GUARD_LIMIT_DEF_A  1.0f   /* H5: boot value, the pack runs' setting; was 1.5 */
#define FRL_GUARD_CMP_MIN_A    0.3f   /* H5: host-writable comparator trip (motorSetVars_M1.overCurrent_A) clamped here */
#define FRL_GUARD_CMP_MAX_A    1.2f
#define FRL_GUARD_BUS_ABS_MIN_V 9.0f   /* the host may set the bus window only inside this range: under the 6 V and */
#define FRL_GUARD_BUS_ABS_MAX_V 22.0f  /* over the 60 V TI faults, above a 20 V tool pack, below the 24 V motors */
#define FRL_GUARD_SPEED_MIN_HZ 10.0f   /* host-writable speed clamp range, electrical */
#define FRL_GUARD_SPEED_MAX_HZ 100.0f  /* 1,500 rpm on the 4-pole-pair BLY171S, rated 4,000 rpm */
#define FRL_GUARD_OFFSET_A     0.10f   /* standstill phase current (each, and their sum) allowed at a start */
#define FRL_GUARD_ISR_HZ       20000U  /* control ISR rate the clock cross-check expects */
#define FRL_GUARD_CLOCK_TOL    0.20f   /* cross-check tolerance: 20 percent over a 1 s window */
#define FRL_GUARD_STOPWATCH_ISRS 40000U /* keep checking current for up to 2 s after a stop */
/* Revision H (2026-10-19): the Hall offset the guard boots with. Unloaded, the current minimum put the offset at 55 to
 * 72 degrees, mean about 62 (results/hall-offset-speed-20261019/RESULTS.md), and 57 degrees gave 6 of 6 starts from
 * the Hall sector (results/faststart-20261019/RESULTS.md). 60 is inside that spread; the curve is flat near its bottom.
 * The host may still write any angle within +/-90; an out-of-range or non-finite write goes back to this default. */
#define FRL_GUARD_HALL_ADJ_DEG 60.0f
/* Revision H4 (2026-10-19): the Hall offset for the calibrated table (frlGuard.hallTable 1). The calibration holds the
 * rotor with d-axis current at each sweep angle and records the angle at which each Hall state is entered, so its edges
 * are already in the FOC frame: the 60 degrees above corrected the default table's frame and is inside the calibrated
 * one. What is left (the dither's 1 degree return, half the 2 degree step, Hall hysteresis, the 150 us debounce at speed)
 * is a few degrees, inside the current minimum's flat bottom (results/hall-offset-speed-20261019/RESULTS.md), so 0. */
#define FRL_GUARD_HALL_ADJ_CAL_DEG 0.0f
/* hall_table_check: a calibrated table is used only if each forward step (3, 2, 6, 4, 5, 1) is 30 to 90 degrees, in
 * both tables, and the reverse edge into a state is within 15 degrees of the forward edge into the next one (the same
 * boundary, measured from the other side) */
#define FRL_GUARD_CAL_STEP_MIN_DEG 30.0f
#define FRL_GUARD_CAL_STEP_MAX_DEG 90.0f
#define FRL_GUARD_CAL_PAIR_DEG     15.0f
/* Revision H5: the DRV8376 cycle-by-cycle current limit. The driver compares the mean of its three sense outputs,
 * VREF/2 + (IA + IB + IC) x GAIN / 3 (currents into the low-side FETs), with the ILIMIT pin, 0 to 4 A over VREF/2 to
 * VREF - 0.25 V, off at VREF or above; the hardware variant coasts and reports every limit event on nFAULT
 * (data sheet SLVSHD4A, 7.3.12 and 9.2.1.1.3), and this port wires nFAULT to the one-shot PWM trip, so a limit event
 * stops the run. ILIMIT is reached from PA4 (CMPSS2's low DAC). With the 3.0 V sense reference and 1 V/A:
 *   on   2.0 V, a 1.5 A sum (1.05 A if the reference were 3.3 V)
 *   test 1.7 V, a 0.6 A sum (0.15 A at 3.3 V): low enough to trip on purpose inside the other limits
 * DAC codes from the commissioning read-back on this board: code 2048 read 1.701 V and 2110 read 1.751 V (0.806 mV per
 * code plus 51 mV). At boot the guard sets the "on" code, reads PA4 back through ADC2 and keeps the limit only if it
 * reads FRL_ILIM_OK_MIN_V to FRL_ILIM_OK_MAX_V; otherwise ILIMIT goes back to full scale (off) and ilimOk reads 0. */
#define FRL_ILIM_CODE_OFF      4095U
#define FRL_ILIM_CODE_ON       2418U
#define FRL_ILIM_CODE_TEST     2048U
#define FRL_ILIM_TEST_MIN_V    1.55f   /* H7: ilimTest_V clamp; the top is the "on" level, so a test is never looser than on */
#define FRL_ILIM_TEST_MAX_V    2.00f
#define FRL_ILIM_V_PER_CODE    0.000806f /* PA4 read-back slope and offset from the commissioning build (2048 read 1.701 V) */
#define FRL_ILIM_V_OFFSET      0.051f
#define FRL_ILIM_OK_MIN_V      1.90f
#define FRL_ILIM_OK_MAX_V      2.10f
/* Revision H5: direction from the rotor. Above FRL_DIR_HOLD_HZ (electrical) of the Hall tracking observer's signed
 * speed, the firmware's direction follows the rotor; below it, the sign of the speed reference, and a zero reference keeps
 * the last direction (the firmware's own rule read 0 as reverse). */
#define FRL_DIR_HOLD_HZ        1.0f
/* Revision H5: the hold test mode (frlGuard.testMode): the guard fixes the FOC angle and the d-axis reference (1) or
 * the d-axis voltage with the current loop bypassed (2), whatever state the controller's start sequence is in. For
 * identification (R from holds at two currents, L from a voltage step through the capture) and for trip tests. */
#define FRL_GUARD_TEST_I_MAX_A 1.15f
#define FRL_GUARD_TEST_V_MAX_V 0.8f
#define FRL_GUARD_TEST_RAMP_MAX 1000.0f  /* A/s; 0 means a step */
#define FRL_GUARD_PERIOD_CYC   (FRL_GUARD_CYC_PER_MS * 1000U / FRL_GUARD_ISR_HZ) /* 10,000 cycles: the 50 us control period */
/* Capture buffer (revision H): FRL_CAP_CH channels of FRL_CAP_N int16 samples, taken in the control ISR. */
#define FRL_CAP_CH             4U
#define FRL_CAP_N              2048U   /* power of two: the ring index is masked, never divided */
#define FRL_CAP_DECIM_MAX      1000U
/* frlGuard.capChan0..3: what a capture channel records (value = int16 sample x capScale, in the unit given) */
enum {
    FRL_CAP_NONE = 0,
    FRL_CAP_IA = 1, FRL_CAP_IB = 2, FRL_CAP_IC = 3,     /* phase currents, halFOCDataObj_M1.adcData.I_A.value[0..2], A */
    FRL_CAP_VA = 4, FRL_CAP_VB = 5, FRL_CAP_VC = 6,     /* phase voltages, halFOCDataObj_M1.adcData.V_V.value[0..2], V */
    FRL_CAP_VBUS = 7,                                   /* bus voltage, halFOCDataObj_M1.adcData.VdcBus_V, V */
    FRL_CAP_HALL = 8,                                   /* Hall state as the controller reads it, hallData_M1.hallState, 0..7 */
    FRL_CAP_ANGLE = 9,                                  /* FOC angle, motorVars_M1.outVars.angleFOC_rad, rad */
    FRL_CAP_ID = 10, FRL_CAP_IQ = 11,                   /* motorVars_M1.outVars.Idq_in_A.value[0..1], A */
    FRL_CAP_IDREF = 12, FRL_CAP_IQREF = 13,             /* motorVars_M1.outVars.IdqRef_A.value[0..1], A */
    FRL_CAP_SPEED = 14,                                 /* motorVars_M1.outVars.speed_Hz, electrical Hz */
    FRL_CAP_HALLRAW = 15,                               /* the three Hall pins read in the ISR, no debounce (bit 0 A, 1 B,
                                                         * 2 C): FRL_CAP_HALL lags an edge by the 3-ISR debounce, 150 us */
    FRL_CAP_PLLSPEED = 16,                              /* revision H3: the Hall tracking observer's speed, frlGuard.pllSpeed_Hz */
    FRL_CAP_PLLANGLE = 17,                              /* revision H3: its angle, rad, offset included, angleAdj_deg not */
    FRL_CAP_CHANNELS = 18
};
/* frlGuard.capTrig */
enum { FRL_TRIG_NOW = 0, FRL_TRIG_RUN_STOP = 1, FRL_TRIG_RUN_START = 2 };
/* frlGuard.capState */
enum { FRL_CAP_IDLE = 0, FRL_CAP_ARMED = 1, FRL_CAP_CAPTURING = 2, FRL_CAP_DONE = 3 };

/* frlGuard.state */
enum { FRL_IDLE = 0, FRL_CALIBRATING = 1, FRL_RUNNING = 2, FRL_STOPPED = 3 };
/* frlGuard.reason: why the last run ended or was refused */
enum {
    FRL_R_NONE = 0, FRL_R_CAP = 1, FRL_R_HOST_LOST = 2, FRL_R_OVERCURRENT = 3, FRL_R_TI_FAULT = 4, FRL_R_BUS = 5,
    FRL_R_REFUSED_FAULT = 6, FRL_R_REFUSED_BUS = 7, FRL_R_REFUSED_UNCALIBRATED = 8, FRL_R_HOST_STOP = 9,
    FRL_R_CAL_DONE = 10, FRL_R_BG_STALL = 11,
    FRL_R_REFUSED_OFFSET = 12, /* a start with the standstill phase currents off zero (offsets taken with the driver off) */
    FRL_R_CLOCK = 13,          /* the cycle counter and the control ISR count disagree: the cap and watchdog clock is wrong */
    FRL_R_STOP_OVERCURRENT = 14 /* over the limit while the firmware was bringing a stopped run down: driver switched off */
};

typedef struct {
    uint16_t version;     /* FRL_GUARD_VERSION; a page checks this before it offers Run */
    uint16_t state;       /* FRL_IDLE ... FRL_STOPPED */
    uint16_t reason;      /* FRL_R_* */
    uint16_t heartbeat;   /* the host writes it; any packet from the host counts as alive */
    uint16_t hall;        /* raw Hall lines, bit 0 A, bit 1 B, bit 2 C (EVM labels), read every millisecond */
    uint16_t hallEdgesA;  /* edges counted on each Hall line since boot (wraps): a line that never moves while the */
    uint16_t hallEdgesB;  /* motor turns is a broken Hall wire, pull-up or supply */
    uint16_t hallEdgesC;
    uint16_t nFault;      /* DRV8376 nFAULT pin: 1 = no fault, 0 = the driver reports a fault */
    uint16_t obsMode;     /* sensorless observer run beside the Halls: 0 off, 1 TI ESMO, 2 TI FAST (host-writable) */
    uint16_t onObs;       /* 1 while the observer's angle and speed drive the FOC instead of the Halls */
    uint16_t hallFree;    /* 1: the observer (minus obsTrimSet_deg) drives from the first instant, through TI's align and
                           * open-loop start; the Halls are read and logged as a referee only (host-writable, idle only) */
    uint32_t runMs;       /* time in the current or last run */
    uint32_t capMs;       /* run cap, host-writable, clamped to FRL_GUARD_CAP_MS_MAX */
    uint32_t hostPackets; /* MotorVision packets received */
    float    peak_A;      /* largest phase current seen in the current or last run */
    float    limit_A;     /* trip level for any phase current, host-writable within the limits above */
    float    speedMax_Hz; /* speed reference clamp, electrical, host-writable within FRL_GUARD_SPEED_MIN/MAX_HZ */
    float    handover_Hz; /* hand the FOC to the observer at or above this Hall speed; 0 = shadow only (host-writable) */
    float    obsSpeed_Hz; /* the observer's speed */
    float    obsErr_deg;  /* observer angle minus Hall angle, low-pass filtered (25 ms), measured whichever drives */
    float    obsErrPk_deg;/* largest |obsErr_deg| in the current or last run while in TI's control-run state */
    float    obsTrim_deg; /* the observer's steady offset from the Hall angle, learned at hand-over and subtracted after */
    float    obsTrimSet_deg;/* the fixed trim used in Hall-free mode (host-writable) */
    float    busMin_V;    /* run refused or stopped below this bus voltage; 10 V for the 12 V adapter, 15.5 V protects a
                           * 20 V tool pack (five cells, 3.1 V each) (host-writable inside the absolute range) */
    float    busMax_V;    /* and above this one; 14.5 V for the adapter, 21 V for the pack */
    float    angleAdj_deg;/* added to the FOC angle in the controller's control-run state, whichever source drives; boots at
                           * FRL_GUARD_HALL_ADJ_DEG (revision H); the current-minimum sweep writes it (host-writable, +/-90) */
    /* ---- revision H (guard version 7) ---- */
    float    isrUs_avg;   /* control ISR duration, microseconds, filtered mean (1/256 per ISR, about 13 ms) */
    float    isrUs_max;   /* longest control ISR since boot, or since the host wrote 0 (host-writable: 0 resets) */
    float    cpuLoad_pct; /* isrUs_avg as a percentage of the 50 us control period: the CPU share the control ISR takes */
    uint16_t capChan[FRL_CAP_CH]; /* FRL_CAP_* per channel (host-writable; read at arming) */
    uint16_t capDecim;    /* 1 = a sample every control ISR, n = every nth (host-writable, 1..FRL_CAP_DECIM_MAX) */
    uint16_t capTrig;     /* FRL_TRIG_* (host-writable) */
    uint16_t capArm;      /* host writes 1 to arm, 2 to cancel; the guard sets it back to 0 when it has acted */
    uint16_t capState;    /* FRL_CAP_* state; arming is refused (it stays as it was) while calibrating or mid-capture */
    uint16_t capWinMs;    /* 0: the window shows capIndex; n: the guard steps capIndex by one every n ms (host-writable) */
    uint32_t capLen;      /* samples per channel to capture (host-writable, 16..FRL_CAP_N) */
    uint32_t capPre;      /* samples before the trigger (host-writable, under capLen; 0 for FRL_TRIG_NOW) */
    uint32_t capCount;    /* samples per channel taken so far, in time order */
    uint32_t capStart;    /* ring position of sample 0: sample i is frlCapBuf[c][(capStart + i) % FRL_CAP_N] */
    uint32_t capIndex;    /* the sample the window shows (host-writable, or stepped by capWinMs) */
    uint32_t capWinIdxA;  /* window sequence marks: written before (A) and after (B) the four values; a reader that */
    uint32_t capWinIdxB;  /* reads B, the values, then A and finds A == B has a consistent sample */
    float    capScale[FRL_CAP_CH]; /* units per int16 count, per channel, set at arming */
    float    capWin[FRL_CAP_CH];   /* sample capIndex of each channel, in units (only once capState is FRL_CAP_DONE) */
    /* ---- revision H3 (guard version 8): the Hall tracking observer (frl_guard.c, hall_pll) ---- */
    uint16_t hallPll;     /* 0: the firmware's own Hall angle and speed drive, as before; 1: the tracking observer's angle
                           * (plus angleAdj_deg) and speed drive the FOC and the speed loop in control run (H3b; H3 also drove
                           * the Hall start), and seed the speed reference once at its first speed, when hallFree is 0 and the sensorless observer is not driving (host-writable,
                           * latched at a run start like the limits; written during a run it goes back) */
    float    pllSpeed_Hz; /* the observer's speed, electrical Hz, signed by the edge order; computed whichever drives */
    float    pllErr_deg;  /* edge angle minus the observer's predicted angle at the last tracked edge */
    /* ---- revision H4 (guard version 9): the calibrated Hall table ---- */
    uint16_t hallTable;   /* 0: the default Hall tables (equal-ish sectors, angleAdj_deg boots at FRL_GUARD_HALL_ADJ_DEG), as
                           * before; 1: the tables the last Hall calibration measured, for the firmware's Hall angle, the
                           * standstill angle and the tracking observer alike (angleAdj_deg FRL_GUARD_HALL_ADJ_CAL_DEG). Writing
                           * it at idle loads that mode's angleAdj_deg default, so write angleAdj_deg after it. A start with 1
                           * and hallTableOk 0 is refused (reason 8). Host-writable, latched at a run start like the limits. */
    uint16_t hallTableOk; /* 1 when the last calibration passed HallCal_validate and the guard's check (hall_table_check) */
    float    calFwd_deg[6]; /* the calibrated forward table, Hall state 1 to 6: the angle at which the state is entered
                             * turning forward (0 until a calibration finishes) */
    float    calRev_deg[6]; /* the reverse table: the angle at which the state is entered turning backward */
    float    calSpread_deg; /* the widest spread (largest minus smallest) of the pole pairs' samples in any of the 12 entries */
    /* ---- revision H5 (guard version 10) ---- */
    uint16_t ilimMode;    /* DRV8376 ILIMIT: 0 off (full scale), 1 on (1.5 A sum), 2 test (0.6 A sum). Boots 1 when the
                           * read-back passed, else 0; host-writable at idle, latched at a run start; 1 and 2 are refused
                           * (back to 0) when ilimOk is 0 */
    uint16_t ilimOk;      /* 1 when the boot read-back of PA4 at the "on" code was inside FRL_ILIM_OK_MIN_V..MAX_V */
    float    ilim_V;      /* that read-back, volts (0 if the ADC was busy) */
    uint16_t dirRotor;    /* 1 (boot): direction from the rotor (FRL_DIR_HOLD_HZ); 0: the firmware's rule, the sign of the
                           * reference. Host-writable, latched at a run start */
    uint16_t testMode;    /* 0 off; 1 current hold: FOC angle testAngle_deg, Id ramped to testId_A, Iq 0; 2 voltage hold:
                           * Vd testVd_V, Vq 0, current loop bypassed. Host-writable, latched at a run start */
    float    dirNow;      /* the direction the firmware uses now, -1 or 1 */
    float    testAngle_deg; /* electrical angle of the hold (latched at a run start) */
    float    testId_A;    /* current-hold target, live during the run, clamped to +/-FRL_GUARD_TEST_I_MAX_A */
    float    testRamp_Aps;/* how fast the applied Id follows testId_A, A/s; 0 = a step (live) */
    float    testVd_V;    /* voltage-hold d-axis voltage, live, clamped to +/-FRL_GUARD_TEST_V_MAX_V */
    float    testIdNow_A; /* the Id reference applied this ISR (current hold) */
    float    cmpLimit_A;  /* the comparator trip level applied (from motorSetVars_M1.overCurrent_A, clamped) */
    uint32_t dirFlips;    /* direction changes the rotor rule made in the current or last run */
    uint16_t ilimCode;    /* the DAC code on PA4 now */
    /* H6 (2026-10-20): a DRV8376 current-limit or over-current shutdown latches nFAULT, and neither the firmware's
     * fault clear nor an MCU reset releases it (results/guard-revH5-20261019). The host writes drvReset 1 at idle;
     * the guard holds DRVOFF high, takes nSLEEP low for 2 ms, raises it, waits 7 ms (tWAKE 5.5 ms max) and writes
     * drvReset back to 0, leaving DRVOFF high: the next start's preflight lowers it (H6b). Not during the post-stop
     * watch; a start is refused while it runs. Clear the firmware's fault word after. */
    uint16_t drvReset;    /* host-writable: 1 asks for a driver sleep and wake (idle only) */
    uint16_t drvResets;   /* sleep-and-wake cycles done since boot */
    /* H7 (2026-10-20): the ILIMIT level for ilimMode 2, volts on PA4, host-writable at idle, clamped to
     * FRL_ILIM_TEST_MIN_V..MAX_V; boots at 1.70 V (code 2046, the H5 test level 2048 within one code) */
    float    ilimTest_V;
} FRL_Guard_t;

extern volatile FRL_Guard_t frlGuard;
extern volatile int16_t frlCapBuf[FRL_CAP_CH][FRL_CAP_N];  /* the capture ring; a debug probe can read it whole */
extern volatile uint32_t frlIsrT0;                          /* cycle counter at the start of the control ISR */

/* first thing in the control ISR: stamp the cycle counter (frl_guard_isr reads it at the end). A macro, so this header
 * needs no CMSIS include of its own; the ISR's file has DWT. */
#define FRL_GUARD_ISR_ENTER() (frlIsrT0 = DWT->CYCCNT)

void frl_guard_boot(void);      /* after SYSCFG_DL_init, before the gate driver is enabled */
void frl_guard_ilim_check(void); /* H5b: ILIMIT read-back, after the offset calibration and before the main loop */
void frl_guard_isr(void);       /* end of the control ISR */
void frl_guard_tick_1ms(void);  /* background, once per millisecond, before TI's low-priority state machine */
void frl_obs_pre(void *motor);  /* control ISR, before TI's Hall read: runs the selected observer in shadow */
void frl_obs_post(void *motor); /* control ISR, after the Hall read: angle error, hand-over and hand-back */
/* revision H5 */
float frl_direction(float speedRef_Hz, float prev); /* low-priority loop: the firmware's direction (application.h) */
void frl_test_angle(void *motor);  /* control ISR, after the state machine: the hold's angle (test mode only) */
void frl_test_ref(void *motor);    /* control ISR, after the speed loop: the hold's current reference */
void frl_test_volt(void *motor);   /* control ISR, after the current loop: the voltage hold */

#endif
