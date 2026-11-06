/*
 * BitNetMCU on the LAUNCHXL-F2800137 (C28x, 120 MHz from TI's device.c): classify the 10 test digits
 * once each with the 64-wide CNN, time each one with CPU Timer 2 counting SYSCLK, and leave the answers
 * and cycle counts in RAM for the debugger (bnm_magic 0xB17E0001 when done), then stop in bench_done().
 * Same method as ../mspm0/main.c. GPL-3.0, like BitNetMCU (upstream/LICENSE).
 */
#include "driverlib.h"
#include "device.h"
#include "bitnet_run.c"

/* CPU Timer 2 counts down from 0xFFFFFFFF at SYSCLK; this returns cycles since start (wraps after 35.8 s) */
static inline unsigned long cyc(void) { return 0xFFFFFFFFUL - CPUTimer_getTimerCount(CPUTIMER2_BASE); }

volatile unsigned long bnm_magic;          /* 0xB17E0001 when done */
volatile unsigned long bnm_clk;            /* SYSCLK in Hz */
volatile unsigned long bnm_overhead;       /* cycles for two back-to-back timer reads, subtracted below */
volatile unsigned long bnm_right;          /* answers that match the label */
volatile unsigned long bnm_pred[N_TEST];
volatile unsigned long bnm_cycles[N_TEST];

__attribute__((noinline)) void bench_done(void) { for (;;) { __asm(" NOP"); } }

void main(void)
{
    Device_init();   /* 120 MHz, watchdog off; FLASH build: wait states, prefetch and cache on */
    bnm_clk = SysCtl_getClock(DEVICE_OSCSRC_FREQ);
    CPUTimer_stopTimer(CPUTIMER2_BASE);
    CPUTimer_selectClockSource(CPUTIMER2_BASE, CPUTIMER_CLOCK_SOURCE_SYS, CPUTIMER_CLOCK_PRESCALER_1);
    CPUTimer_setPeriod(CPUTIMER2_BASE, 0xFFFFFFFFUL);
    CPUTimer_setPreScaler(CPUTIMER2_BASE, 0);
    CPUTimer_reloadTimerCounter(CPUTIMER2_BASE);
    CPUTimer_startTimer(CPUTIMER2_BASE);
    { unsigned long a = cyc(); unsigned long b = cyc(); bnm_overhead = b - a; }
    int i;
    for (i = 0; i < N_TEST; i++) {
        unsigned long t0 = cyc();
        uint32_t p = BitMnistInference(test_in[i]);
        unsigned long t1 = cyc();
        bnm_pred[i] = p;
        bnm_cycles[i] = (t1 - t0) - bnm_overhead;
        bnm_right += (p == *test_label[i]);
    }
    bnm_magic = 0xB17E0001UL;
    bench_done();
}
