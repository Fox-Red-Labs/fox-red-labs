/*
 * Fox Red Labs: the 8k room-occupancy model inside TI's basic_ble, on the LP-EM-CC2340R5.
 * Phase 4 follow-up (docs/superpowers/plans/2026-10-11-phase4-composition.md): does Bluetooth plus the model
 * fit, and does the model still give the right answers while the Bluetooth stack runs?
 *
 * A low-priority FreeRTOS task, once a second:
 *   - runs the 20 known windows, counts right classes and logits that differ from onnxruntime's answers;
 *   - times 100 answers (5 passes of the 20) with the TI driver clock;
 *   - measures its own stack use from a painted stack.
 * Results sit in g_occ in RAM; the debugger reads them (no serial output, so no pin besides the
 * debugger's is driven). magic 0x0CC08C8B marks the block; rounds climbs once a second while it runs.
 * Model: mspm0g3507/mcu-bench/edgeai/uci-room-occupancy/occ_cnn_8k, model.a (-Os, TI Arm Clang 4.0.4,
 * Cortex-M0+), unchanged.
 */
#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>
#include <ti/drivers/dpl/ClockP.h>
#include "occ_cnn_8k/occ_cnn_8k.h"
#include "occ_cnn_8k/test_vectors.h"

#ifndef OCC_STACK_WORDS
#define OCC_STACK_WORDS 512
#endif

typedef struct {
    uint32_t magic;          /* 0x0CC08C8B */
    uint32_t rounds;         /* completed rounds, one a second */
    uint32_t class_ok;       /* of 20, last round */
    uint32_t bad;            /* wrong classes plus logits that differ, last round */
    uint32_t us_per_answer;  /* mean of 100 answers, last round */
    uint32_t stack_used;     /* bytes of OCC_STACK_WORDS*4 ever touched */
    uint32_t stack_size;
    uint32_t tick_us;        /* the driver clock's tick, for the record */
} occ_result_t;

volatile occ_result_t g_occ = { 0x0CC08C8Bu, 0, 0, 0, 0, 0, OCC_STACK_WORDS * 4, 0 };

static StackType_t occ_stack[OCC_STACK_WORDS];
static StaticTask_t occ_tcb;

static uint32_t stack_used(void)
{
    const uint8_t *p = (const uint8_t *)occ_stack, *end = p + sizeof occ_stack;
    while (p < end && *p == 0xA5) p++;          /* the stack grows down from the end */
    return (uint32_t)(end - p);
}

static void occ_task(void *arg)
{
    (void)arg;
    int8_t lg[4];
    g_occ.tick_us = ClockP_getSystemTickPeriod();
    for (;;) {
        uint32_t ok = 0, bad = 0;
        for (int v = 0; v < 20; v++) {
            int c = occ_cnn_8k_run(occ_cnn_8k_test_x[v], lg);
            ok += (c == occ_cnn_8k_test_expected_class[v]);
            bad += (c != occ_cnn_8k_test_expected_class[v]);
            for (int k = 0; k < 4; k++) bad += (lg[k] != occ_cnn_8k_test_expected_logits[v][k]);
        }
        uint32_t t0 = ClockP_getSystemTicks();
        for (int r = 0; r < 5; r++)
            for (int v = 0; v < 20; v++) (void)occ_cnn_8k_run(occ_cnn_8k_test_x[v], lg);
        uint32_t t1 = ClockP_getSystemTicks();
        g_occ.class_ok = ok;
        g_occ.bad = bad;
        g_occ.us_per_answer = (t1 - t0) * g_occ.tick_us / 100u;
        g_occ.stack_used = stack_used();
        g_occ.rounds++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void occ_start(void)
{
    uint8_t *p = (uint8_t *)occ_stack;
    for (unsigned i = 0; i < sizeof occ_stack; i++) p[i] = 0xA5;
    (void)xTaskCreateStatic(occ_task, "occ", OCC_STACK_WORDS, NULL, 1, occ_stack, &occ_tcb);
}
