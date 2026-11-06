/*
 * FreeRTOS timing on the LP-MSPM0L1306, in CPU cycles at 32 MHz.
 *
 * Cycle counter: FreeRTOS owns SysTick (its tick), so this uses TIMG0, a 16-bit timer clocked from
 * BUSCLK (32 MHz, divide 1, no prescale) counting up from 0 to 0xFFFF and wrapping. One count is one
 * CPU cycle; differences are taken modulo 65,536, so a single measurement must stay under 2.048 ms
 * (every test here is under a few hundred cycles). Before the scheduler starts, the program times
 * one delay with both TIMG0 and SysTick to prove TIMG0 runs at the CPU clock.
 *
 * Tasks: bench (priority 1, measures), peer (priority 1, only for the yield test), hi (priority 2,
 * waits on each object in turn). Each test runs W warm-up passes, then N measured passes.
 * Interrupt: I2C1 (line 25, not used by anything else), pended by software, priority 1.
 *
 * Results in RAM (struct fr_bench), read with the debugger. magic = 0xBE4C0001 when every test
 * has finished (well under a second after reset). Per test: best, worst, worst of the passes with
 * no tick interrupt inside, and the sum of all N (average = sum / N). All raw: each includes one
 * TIMG0 read, whose cost is in timer_read.
 */
#include "ti_msp_dl_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "fr_common.h"

#define N 1000
#define W 8
#define BENCH_STACK_WORDS 96
#define PEER_STACK_WORDS  configMINIMAL_STACK_SIZE
#define HI_STACK_WORDS    64

typedef struct {
    uint32_t best;
    uint32_t worst;
    uint32_t worst_no_tick;
    uint32_t sum;
} stat_t;

volatile struct {
    uint32_t magic;          /* 0xBE4C0001 when done */
    uint32_t samples;        /* N per test */
    uint32_t timg_counts;    /* TIMG0 counts across one delay_cycles(20000), before the scheduler */
    uint32_t systick_cycles; /* SysTick (CPU clock) cycles across the same delay: should match */
    uint32_t timer_read;     /* cycles between two back-to-back TIMG0 reads, best of 16 */
    stat_t yield_rt;         /* taskYIELD round trip: bench yields, peer runs and yields back */
    stat_t sem_wake;         /* xSemaphoreGive to the first line of the higher-priority taker */
    stat_t notify_wake;      /* xTaskNotifyGive to the first line of the higher-priority waiter */
    stat_t queue_rt;         /* xQueueSend to hi, hi sends back, bench's xQueueReceive returns */
    stat_t irq_entry;        /* pend I2C1 to the first line of its handler */
    stat_t irq_sem_task;     /* pend to the first line of the task woken by xSemaphoreGiveFromISR */
    stat_t irq_notify_task;  /* pend to the first line of the task woken by vTaskNotifyGiveFromISR */
    uint32_t errors;         /* passes where the other task did not run exactly once, or a queue
                                reply did not match: must be 0 */
    uint32_t stack_words[4]; /* bench, peer, hi, idle */
    uint32_t hwm_words[4];   /* stack words never used, same order */
    uint32_t ticks_end;      /* FreeRTOS tick count when done */
} fr_bench;

static StaticTask_t bench_tcb, peer_tcb, hi_tcb;
static StackType_t bench_stack[BENCH_STACK_WORDS];
static StackType_t peer_stack[PEER_STACK_WORDS];
static StackType_t hi_stack[HI_STACK_WORDS];
static StaticSemaphore_t sem_buf;
static StaticQueue_t q1_buf, q2_buf;
static uint8_t q1_store[sizeof(uint32_t)], q2_store[sizeof(uint32_t)];
static SemaphoreHandle_t sem;
static QueueHandle_t q1, q2;
static TaskHandle_t hi_handle;

static volatile uint8_t yield_done;
static volatile uint8_t isr_notify; /* 0: ISR gives the semaphore, 1: ISR notifies hi */
static volatile uint16_t t_hi, t_isr;
static volatile uint32_t hi_passes, peer_passes; /* lock-step check, read outside the timed window */

static inline uint16_t cyc(void)
{
    return (uint16_t) TIMG0->COUNTERREGS.CTR;
}

static void cyc_init(void)
{
    static const DL_TimerG_ClockConfig clk = {
        .clockSel = DL_TIMER_CLOCK_BUSCLK, .divideRatio = DL_TIMER_CLOCK_DIVIDE_1, .prescale = 0};
    static const DL_TimerG_TimerConfig tim = {
        .timerMode = DL_TIMER_TIMER_MODE_PERIODIC_UP, .period = 0xFFFF, .startTimer = DL_TIMER_STOP};
    DL_TimerG_reset(TIMG0);
    DL_TimerG_enablePower(TIMG0);
    delay_cycles(POWER_STARTUP_DELAY);
    DL_TimerG_setClockConfig(TIMG0, (DL_TimerG_ClockConfig *) &clk);
    DL_TimerG_initTimerMode(TIMG0, (DL_TimerG_TimerConfig *) &tim);
    DL_TimerG_enableClock(TIMG0);
    DL_TimerG_startCounter(TIMG0);
}

static void stat_init(volatile stat_t *s)
{
    s->best = UINT32_MAX;
    s->worst = 0;
    s->worst_no_tick = 0;
    s->sum = 0;
}

static void stat_add(volatile stat_t *s, uint16_t t0, uint16_t t1, TickType_t k0)
{
    uint32_t dt = (uint16_t) (t1 - t0);
    if (dt < s->best) {
        s->best = dt;
    }
    if (dt > s->worst) {
        s->worst = dt;
    }
    if (xTaskGetTickCount() == k0 && dt > s->worst_no_tick) {
        s->worst_no_tick = dt;
    }
    s->sum += dt;
}

void I2C1_IRQHandler(void)
{
    BaseType_t woken = pdFALSE;
    t_isr = cyc();
    if (isr_notify) {
        vTaskNotifyGiveFromISR(hi_handle, &woken);
    } else {
        xSemaphoreGiveFromISR(sem, &woken);
    }
    portYIELD_FROM_ISR(woken);
}

static void pend_irq(void)
{
    NVIC_SetPendingIRQ(I2C1_INT_IRQn);
    __DSB();
    __ISB();
}

/* Higher priority: each loop below matches one test in bench_task, pass for pass. */
static void hi_task(void *arg)
{
    uint32_t v;
    (void) arg;
    for (int i = 0; i < N + W; i++) { /* sem_wake */
        xSemaphoreTake(sem, portMAX_DELAY);
        t_hi = cyc();
        hi_passes++;
    }
    for (int i = 0; i < N + W; i++) { /* notify_wake */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        t_hi = cyc();
        hi_passes++;
    }
    for (int i = 0; i < N + W; i++) { /* queue_rt */
        xQueueReceive(q1, &v, portMAX_DELAY);
        xQueueSend(q2, &v, portMAX_DELAY);
    }
    for (int i = 0; i < N + W; i++) { /* irq_sem_task */
        xSemaphoreTake(sem, portMAX_DELAY);
        t_hi = cyc();
        hi_passes++;
    }
    for (int i = 0; i < N + W; i++) { /* irq_notify_task */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        t_hi = cyc();
        hi_passes++;
    }
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

/* Same priority as bench: counts the pass and hands the CPU straight back on every yield. The
 * count (one load, add, store) is inside the timed round trip, as in Thread-Metric's tests. */
static void peer_task(void *arg)
{
    (void) arg;
    while (!yield_done) {
        peer_passes++;
        taskYIELD();
    }
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

#define LOCKSTEP(count, before) \
    do { \
        if ((count) != (before) + 1) { \
            fr_bench.errors++; \
        } \
    } while (0)

static void bench_task(void *arg)
{
    uint16_t t0, t1;
    TickType_t k0;
    uint32_t p;
    (void) arg;

    for (int i = 0; i < N + W; i++) {
        p = peer_passes;
        k0 = xTaskGetTickCount();
        t0 = cyc();
        taskYIELD();
        t1 = cyc();
        LOCKSTEP(peer_passes, p);
        if (i >= W) {
            stat_add(&fr_bench.yield_rt, t0, t1, k0);
        }
    }
    yield_done = 1;
    taskYIELD(); /* peer sees the flag and parks */

    for (int i = 0; i < N + W; i++) {
        p = hi_passes;
        k0 = xTaskGetTickCount();
        t0 = cyc();
        xSemaphoreGive(sem);
        LOCKSTEP(hi_passes, p);
        if (i >= W) {
            stat_add(&fr_bench.sem_wake, t0, t_hi, k0);
        }
    }

    for (int i = 0; i < N + W; i++) {
        p = hi_passes;
        k0 = xTaskGetTickCount();
        t0 = cyc();
        xTaskNotifyGive(hi_handle);
        LOCKSTEP(hi_passes, p);
        if (i >= W) {
            stat_add(&fr_bench.notify_wake, t0, t_hi, k0);
        }
    }

    for (int i = 0; i < N + W; i++) {
        uint32_t v = 0x5A000000u + (uint32_t) i, r = 0;
        k0 = xTaskGetTickCount();
        t0 = cyc();
        xQueueSend(q1, &v, portMAX_DELAY);
        xQueueReceive(q2, &r, portMAX_DELAY);
        t1 = cyc();
        if (r != v) {
            fr_bench.errors++;
        }
        if (i >= W) {
            stat_add(&fr_bench.queue_rt, t0, t1, k0);
        }
    }

    isr_notify = 0;
    for (int i = 0; i < N + W; i++) {
        p = hi_passes;
        k0 = xTaskGetTickCount();
        t0 = cyc();
        pend_irq();
        LOCKSTEP(hi_passes, p);
        if (i >= W) {
            stat_add(&fr_bench.irq_entry, t0, t_isr, k0);
            stat_add(&fr_bench.irq_sem_task, t0, t_hi, k0);
        }
    }

    isr_notify = 1;
    for (int i = 0; i < N + W; i++) {
        p = hi_passes;
        k0 = xTaskGetTickCount();
        t0 = cyc();
        pend_irq();
        LOCKSTEP(hi_passes, p);
        if (i >= W) {
            stat_add(&fr_bench.irq_notify_task, t0, t_hi, k0);
        }
    }

    fr_bench.hwm_words[0] = uxTaskGetStackHighWaterMark(NULL);
    fr_bench.hwm_words[1] = uxTaskGetStackHighWaterMark((TaskHandle_t) &peer_tcb);
    fr_bench.hwm_words[2] = uxTaskGetStackHighWaterMark(hi_handle);
    fr_bench.hwm_words[3] = uxTaskGetStackHighWaterMark(xTaskGetIdleTaskHandle());
    fr_bench.ticks_end = xTaskGetTickCount();
    fr_bench.magic = 0xBE4C0001u;
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

int main(void)
{
    SYSCFG_DL_init();
    fr_led1_init();
    cyc_init();

    /* TIMG0 against SysTick (CPU clock) over the same delay. SysTick is free until the
     * scheduler starts; it is switched off again before FreeRTOS sets it up for its tick. */
    SysTick->LOAD = 0xFFFFFFu;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    uint32_t s0 = SysTick->VAL;
    uint16_t c0 = cyc();
    delay_cycles(20000);
    uint32_t s1 = SysTick->VAL;
    uint16_t c1 = cyc();
    SysTick->CTRL = 0;
    fr_bench.systick_cycles = s0 - s1;
    fr_bench.timg_counts = (uint16_t) (c1 - c0);

    uint32_t best = UINT32_MAX;
    for (int i = 0; i < 16; i++) {
        uint16_t a = cyc();
        uint16_t b = cyc();
        if ((uint16_t) (b - a) < best) {
            best = (uint16_t) (b - a);
        }
    }
    fr_bench.timer_read = best;
    fr_bench.samples = N;
    stat_init(&fr_bench.yield_rt);
    stat_init(&fr_bench.sem_wake);
    stat_init(&fr_bench.notify_wake);
    stat_init(&fr_bench.queue_rt);
    stat_init(&fr_bench.irq_entry);
    stat_init(&fr_bench.irq_sem_task);
    stat_init(&fr_bench.irq_notify_task);
    fr_bench.stack_words[0] = BENCH_STACK_WORDS;
    fr_bench.stack_words[1] = PEER_STACK_WORDS;
    fr_bench.stack_words[2] = HI_STACK_WORDS;
    fr_bench.stack_words[3] = configMINIMAL_STACK_SIZE;

    sem = xSemaphoreCreateBinaryStatic(&sem_buf);
    q1 = xQueueCreateStatic(1, sizeof(uint32_t), q1_store, &q1_buf);
    q2 = xQueueCreateStatic(1, sizeof(uint32_t), q2_store, &q2_buf);
    hi_handle = xTaskCreateStatic(hi_task, "hi", HI_STACK_WORDS, NULL, 2, hi_stack, &hi_tcb);
    xTaskCreateStatic(peer_task, "peer", PEER_STACK_WORDS, NULL, 1, peer_stack, &peer_tcb);
    xTaskCreateStatic(bench_task, "bnch", BENCH_STACK_WORDS, NULL, 1, bench_stack, &bench_tcb);

    NVIC_ClearPendingIRQ(I2C1_INT_IRQn);
    NVIC_SetPriority(I2C1_INT_IRQn, 1);
    NVIC_EnableIRQ(I2C1_INT_IRQn);

    vTaskStartScheduler();
    for (;;) {
    }
}
