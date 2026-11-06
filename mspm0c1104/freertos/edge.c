/*
 * How many FreeRTOS tasks fit in the LP-MSPM0C1104's 1 kB of RAM?
 *
 * A "minimal task" has a stack of configMINIMAL_STACK_SIZE words (the same as the idle task) and
 * does the least a real task does: it writes a flag and sleeps one tick, forever.
 *
 * Static image (FR_HEAP_BYTES == 0): EDGE_TASKS minimal tasks are arrays in .bss. build.sh raises
 * EDGE_TASKS until the linker refuses, then keeps the largest count that links.
 * Heap image (FR_HEAP_BYTES > 0): the heap (heap_4) is the largest that links. The checker task
 * creates minimal tasks with xTaskCreate until it fails for lack of memory.
 *
 * In both, a checker task (priority 2, CHECK_STACK_WORDS) looks once a second at which minimal
 * tasks ran since the last look, and at every task's stack high-water mark.
 *
 * Results in RAM (struct fr_edge), read with the debugger. magic = 0xED6E0001 after the 2nd check
 * (2 s after reset); checks keeps counting once a second.
 */
#include "ti_msp_dl_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "fr_common.h"

#ifndef EDGE_TASKS
#define EDGE_TASKS 8
#endif
#define EDGE_MAX 24 /* heap image: stop trying after this many */
#ifndef CHECK_STACK_WORDS
#define CHECK_STACK_WORDS 56 /* stack analysis on the L1306: 56 words needed with static memory */
#endif

volatile struct {
    uint32_t magic;         /* 0xED6E0001 after the 2nd check */
    uint32_t checks;        /* checks done, one per second */
    uint32_t made;          /* minimal tasks created */
    uint32_t alive;         /* minimal tasks that ran since the previous check */
    uint32_t stack_words;   /* stack of each minimal task, words (configMINIMAL_STACK_SIZE) */
    uint32_t hwm_minimal;   /* fewest stack words never used, over all minimal tasks */
    uint32_t hwm_checker;   /* checker: stack words never used (of CHECK_STACK_WORDS) */
    uint32_t hwm_idle;      /* idle task: stack words never used (of configMINIMAL_STACK_SIZE) */
    uint32_t heap_total;    /* heap image: configTOTAL_HEAP_SIZE, bytes */
    uint32_t heap_free;     /* heap image: free after the last create, bytes */
    uint32_t heap_min_ever; /* heap image: xPortGetMinimumEverFreeHeapSize, bytes */
    uint32_t ran_out;       /* heap image: 1 if xTaskCreate failed (heap full), 0 if EDGE_MAX came first */
} fr_edge;

#if FR_HEAP_BYTES == 0
static StaticTask_t tcb[EDGE_TASKS];
static StackType_t stk[EDGE_TASKS][configMINIMAL_STACK_SIZE];
static uint8_t ran[EDGE_TASKS];
static StaticTask_t check_tcb;
static StackType_t check_stk[CHECK_STACK_WORDS];
#define HANDLE(i) ((TaskHandle_t) &tcb[i]) /* a static task's handle is its block's address */
#else
static TaskHandle_t handles[EDGE_MAX];
static uint8_t ran[EDGE_MAX];
#define HANDLE(i) (handles[i])
#endif

static void minimal_task(void *flag)
{
    for (;;) {
        *(volatile uint8_t *) flag = 1;
        vTaskDelay(1);
    }
}

static void checker_task(void *arg)
{
    uint32_t n;
    (void) arg;
#if FR_HEAP_BYTES == 0
    n = EDGE_TASKS;
#else
    n = 0;
    while (n < EDGE_MAX && xTaskCreate(minimal_task, "min", configMINIMAL_STACK_SIZE, &ran[n], 1,
                               &handles[n]) == pdPASS) {
        n++;
    }
    fr_edge.ran_out = (n < EDGE_MAX);
    fr_edge.heap_total = configTOTAL_HEAP_SIZE;
    fr_edge.heap_free = xPortGetFreeHeapSize();
#endif
    fr_edge.made = n;
    fr_edge.stack_words = configMINIMAL_STACK_SIZE;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        uint32_t alive = 0, low = UINT32_MAX;
        for (uint32_t i = 0; i < n; i++) {
            alive += ran[i];
            ran[i] = 0;
            uint32_t h = uxTaskGetStackHighWaterMark(HANDLE(i));
            if (h < low) {
                low = h;
            }
        }
        fr_edge.alive = alive;
        fr_edge.hwm_minimal = low;
        fr_edge.hwm_checker = uxTaskGetStackHighWaterMark(NULL);
        fr_edge.hwm_idle = uxTaskGetStackHighWaterMark(xTaskGetIdleTaskHandle());
#if FR_HEAP_BYTES > 0
        fr_edge.heap_min_ever = xPortGetMinimumEverFreeHeapSize();
#endif
        fr_edge.checks++;
        if (fr_edge.checks >= 2) {
            fr_edge.magic = 0xED6E0001u;
        }
    }
}

int main(void)
{
    SYSCFG_DL_init();
    fr_led1_init();
#if FR_HEAP_BYTES == 0
    for (int i = 0; i < EDGE_TASKS; i++) {
        xTaskCreateStatic(minimal_task, "min", configMINIMAL_STACK_SIZE, &ran[i], 1, stk[i], &tcb[i]);
    }
    xTaskCreateStatic(checker_task, "chk", CHECK_STACK_WORDS, NULL, 2, check_stk, &check_tcb);
#else
    xTaskCreate(checker_task, "chk", CHECK_STACK_WORDS, NULL, 2, NULL);
#endif
    vTaskStartScheduler();
    for (;;) {
    }
}
