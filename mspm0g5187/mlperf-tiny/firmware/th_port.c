/*
 * MLPerf Tiny device-side port for the LP-MSPM0G5187 (Cortex-M0+ at 80 MHz, TinyEngine NPU).
 * Implements the th_* functions of third-party/mlperf-tiny/api/submitter_implemented.h on the backchannel UART
 * (UC0, PA10 TX and PA11 RX, 115200 baud, polled), so the unmodified EEMBC host runner (third-party/mlperf-tiny/runner)
 * can drive it: name, db load, db <hex>, infer N W, results, timestamp.
 *   -DTASK_KWS   keyword spotting, TI-NPU-quantized DS-CNN from host/kws_tinpu.py, compiled by NNC with
 *                output_int=true (float MFCC in, int8 logits out).
 * The host sends the benchmark's int8 bytes (quantized with the reference int8 model's input scale and zero point);
 * th_load_tensor turns each back into the float MFCC value, once per `infer` command, outside the timed loop, as in the
 * benchmark's reference ports. The model's own first layers (pad, scale, round, clip to int8) run inside the timed call:
 * NNC cannot drop them with skip_normalize because the pad comes first, so the model takes float input and returns int8 logits.
 * th_timestamp prints SysTick cycles / 80 as microseconds.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "ti_msp_dl_config.h"
#include "../../../mspm0g3507/mcu-bench/sdk-common/bench_timer.h"
#include "api/internally_implemented.h"
#include "api/submitter_implemented.h"
#include "tvmgen_default.h"

#define NIN 490
#define NOUT 12
extern uint8_t gp_buff[];
static float g_in[NIN] __attribute__((aligned(4)));
#define IN_SCALE 0.5847029f   /* input scale and zero point of the benchmark's reference int8 model: the host sends q = x/scale + zero point */
#define IN_ZP 83
static int8_t g_out[NOUT] __attribute__((aligned(4)));

static void put(char c) { while (DL_UART_Main_isTXFIFOFull(UART_0_INST)) { } DL_UART_Main_transmitData(UART_0_INST, (uint8_t)c); }
void th_serialport_initialize(void) { }
void th_timestamp_initialize(void) { bench_timer_start(); }
void th_final_initialize(void)
{
    DL_NPU_reset(NPU);
    DL_NPU_enablePower(NPU);
    while (!(DL_SYSCTL_getStatus() & DL_SYSCTL_STATUS_NPU_READY)) { }
    DL_NPU_clearInterruptStatus(NPU, DL_NPU_INTERRUPT_DONE);
    DL_NPU_enableInterrupt(NPU, DL_NPU_INTERRUPT_DONE);
    NVIC_EnableIRQ(NPU_INT_IRQn);
}
char th_getchar(void)
{
    while (DL_UART_Main_isRXFIFOEmpty(UART_0_INST)) { }
    return (char)(UART_0_INST->uart->RXDATA & 0xFF);
}
int th_vprintf(const char *fmt, va_list ap)
{
    char b[128]; int n = vsnprintf(b, sizeof b, fmt, ap);
    for (int i = 0; i < n && i < (int)sizeof b - 1; i++) put(b[i]);
    return n;
}
void th_printf(const char *fmt, ...) { va_list ap; va_start(ap, fmt); th_vprintf(fmt, ap); va_end(ap); }
void th_timestamp(void) { th_printf(EE_MSG_TIMESTAMP, (unsigned long)(bench_cycles() / (CPUCLK_FREQ / 1000000u))); }
void th_command_ready(char volatile *msg) { ee_serial_command_parser_callback((char *)msg); }
void th_pre(void) { }
void th_post(void) { }
void th_load_tensor(void) { for (int i = 0; i < NIN; i++) g_in[i] = (float)((int)(int8_t)gp_buff[i] - IN_ZP) * IN_SCALE; }
void th_infer(void)
{
    struct tvmgen_default_inputs ti = { (void *)g_in };
    struct tvmgen_default_outputs to = { (void *)g_out };
    tvmgen_default_run(&ti, &to);
#ifdef TVMGEN_DEFAULT_TI_NPU
    while (!tvmgen_default_finished) { }
#endif
}
void th_results(void)
{
    th_printf("m-results-[");
    for (int i = 0; i < NOUT; i++) th_printf(i ? ",%d" : "%d", (int)g_out[i]);
    th_printf("]\r\n");
}
int th_strncmp(const char *a, const char *b, size_t n) { return strncmp(a, b, n); }
char *th_strncpy(char *d, const char *s, size_t n) { return strncpy(d, s, n); }
size_t th_strnlen(const char *s, size_t n) { size_t i = 0; while (i < n && s[i]) i++; return i; }
char *th_strcat(char *d, const char *s) { return strcat(d, s); }
char *th_strtok(char *s, const char *sep) { return strtok(s, sep); }
int th_atoi(const char *s) { return atoi(s); }
void *th_memset(void *b, int c, size_t n) { return memset(b, c, n); }
void *th_memcpy(void *d, const void *s, size_t n) { return memcpy(d, s, n); }

#ifdef SWD_HARNESS
/* No UART: the Mac writes one clip into gp_buff through the debugger's memory port while the core runs, sets swd.go,
 * and reads the int8 logits and the cycle count back when swd.go is 0 again. The core is never halted or reset, because a
 * halt during an NPU run can leave the NPU stuck (mspm0g5187/AGENTS.md). Used when the UART pins carry a loop wire. */
volatile struct { uint32_t magic, go, warm, n; uint32_t cycles; int8_t out[16]; } swd;
int main(void)
{
    SYSCFG_DL_init();
    th_timestamp_initialize(); th_final_initialize();
    swd.magic = 0x5EDA0001u;
    for (;;) {
        while (!swd.go) { }
        th_load_tensor();
        for (uint32_t i = 0; i < swd.warm; i++) th_infer();
        uint64_t t0 = bench_cycles();
        for (uint32_t i = 0; i < swd.n; i++) th_infer();
        swd.cycles = (uint32_t)(bench_cycles() - t0);
        for (int i = 0; i < NOUT; i++) swd.out[i] = g_out[i];
        swd.go = 0;
    }
}
#else
int main(void)
{
    SYSCFG_DL_init();
    ee_benchmark_initialize();
    for (;;) ee_serial_callback(th_getchar());
}
#endif
