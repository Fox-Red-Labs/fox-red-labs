/*
 * FreeRTOS timing on the LP-MSPM0C1104, in CPU cycles at 24 MHz. Cut down from the LP-MSPM0L1306 test
 * (../../mspm0l1306/freertos/bench.c) because the chip has room for one application task plus the idle task and no more.
 * Same method: 8 warm-up passes, then 1,000 measured; best, worst, worst with no tick interrupt inside, sum.
 *
 * Cycle counter: TIMG8, 16 bits on BUSCLK. FreeRTOS's tick is TIMG14 (the chip has no working SysTick).
 *
 * Tests:
 *  yield_idle  the bench task (priority 0, the idle task's priority) calls taskYIELD(); the idle task runs one pass of its
 *              loop and yields back (configIDLE_SHOULD_YIELD 1). Two switches plus one idle-loop pass. Not comparable
 *              one for one with the L1306's yield to an equal-priority peer; the idle pass adds some cycles.
 *  isr_to_task a one-shot TIMA0 interrupt notifies the waiting bench task (priority 1, the idle task runs meanwhile):
 *              from the handler's first line to the task's first line after ulTaskNotifyTake returns.
 *  isr_entry   TIMA0 reaching zero to the handler's first line is not observable; the bare-metal figure is in the
 *              C1104 results, so it is not repeated here.
 * Results in RAM (struct fr_bench), read with the debugger; magic 0xBE4C0001 when done.
 */
#include "ti_msp_dl_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "fr_common.h"

#ifndef N
#define N 1000
#endif
#define W 8

typedef struct {
    uint32_t best, worst, worst_no_tick, sum;
} stat_t;

volatile struct {
    uint32_t magic;
    uint32_t timg_counts;    /* TIMG8 counts across one delay_cycles(20000): should read about 20,000 */
    uint32_t timer_read;     /* cycles between two back-to-back TIMG8 reads, best of 16 */
    stat_t yield_idle, isr_to_task;
    uint32_t hwm_words[2];   /* bench task, idle task: stack words never used */
    uint32_t ticks_end;
} fr_bench;

static StaticTask_t bench_tcb;
static StackType_t bench_stack[configMINIMAL_STACK_SIZE];
static TaskHandle_t bench_handle;
static volatile uint16_t t_isr;

static inline uint16_t cyc(void) { return (uint16_t) TIMG8->COUNTERREGS.CTR; }

static void cyc_init(void)
{
    static const DL_TimerG_ClockConfig clk = {.clockSel = DL_TIMER_CLOCK_BUSCLK, .divideRatio = DL_TIMER_CLOCK_DIVIDE_1, .prescale = 0};
    static const DL_TimerG_TimerConfig tim = {.timerMode = DL_TIMER_TIMER_MODE_PERIODIC_UP, .period = 0xFFFF, .startTimer = DL_TIMER_STOP};
    DL_TimerG_reset(TIMG8);
    DL_TimerG_enablePower(TIMG8);
    delay_cycles(POWER_STARTUP_DELAY);
    DL_TimerG_setClockConfig(TIMG8, (DL_TimerG_ClockConfig *) &clk);
    DL_TimerG_initTimerMode(TIMG8, (DL_TimerG_TimerConfig *) &tim);
    DL_TimerG_startCounter(TIMG8);
}

static void oneshot_init(void)
{
    static const DL_TimerA_ClockConfig clk = {.clockSel = DL_TIMER_CLOCK_BUSCLK, .divideRatio = DL_TIMER_CLOCK_DIVIDE_1, .prescale = 0};
    static const DL_TimerA_TimerConfig tim = {.timerMode = DL_TIMER_TIMER_MODE_ONE_SHOT, .period = 600, .startTimer = DL_TIMER_STOP};
    DL_TimerA_reset(TIMA0);
    DL_TimerA_enablePower(TIMA0);
    delay_cycles(POWER_STARTUP_DELAY);
    DL_TimerA_setClockConfig(TIMA0, (DL_TimerA_ClockConfig *) &clk);
    DL_TimerA_initTimerMode(TIMA0, (DL_TimerA_TimerConfig *) &tim);
    DL_TimerA_enableInterrupt(TIMA0, DL_TIMERA_INTERRUPT_ZERO_EVENT);
    NVIC_SetPriority(TIMA0_INT_IRQn, 1);
    NVIC_EnableIRQ(TIMA0_INT_IRQn);
}

static void stat_init(volatile stat_t *s) { s->best = UINT32_MAX; s->worst = 0; s->worst_no_tick = 0; s->sum = 0; }

static void stat_add(volatile stat_t *s, uint16_t t0, uint16_t t1, TickType_t k0)
{
    uint32_t dt = (uint16_t) (t1 - t0);
    if (dt < s->best) s->best = dt;
    if (dt > s->worst) s->worst = dt;
    if (xTaskGetTickCount() == k0 && dt > s->worst_no_tick) s->worst_no_tick = dt;
    s->sum += dt;
}

void TIMA0_IRQHandler(void)
{
    BaseType_t woken = pdFALSE;
    t_isr = cyc();
    (void) TIMA0->CPU_INT.IIDX; /* reading IIDX clears the event */
    vTaskNotifyGiveFromISR(bench_handle, &woken);
    portYIELD_FROM_ISR(woken);
}

static void bench_task(void *arg)
{
    (void) arg;
    TickType_t k0;
    uint16_t t0, t1;
    for (int i = 0; i < N + W; i++) {
        k0 = xTaskGetTickCount();
        t0 = cyc(); taskYIELD(); t1 = cyc();
        if (i >= W) stat_add(&fr_bench.yield_idle, t0, t1, k0);
    }
    vTaskPrioritySet(NULL, 1);
    for (int i = 0; i < N + W; i++) {
        k0 = xTaskGetTickCount();
        DL_TimerA_setLoadValue(TIMA0, 600);
        DL_TimerA_startCounter(TIMA0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        t1 = cyc();
        if (i >= W) stat_add(&fr_bench.isr_to_task, t_isr, t1, k0);
    }
    fr_bench.hwm_words[0] = uxTaskGetStackHighWaterMark(NULL);
    fr_bench.hwm_words[1] = uxTaskGetStackHighWaterMark(xTaskGetIdleTaskHandle());
    fr_bench.ticks_end = xTaskGetTickCount();
    fr_bench.magic = 0xBE4C0001u;
    for (;;) vTaskDelay(portMAX_DELAY);
}

int main(void)
{
    SYSCFG_DL_init();
    fr_led1_init();
    cyc_init();
    oneshot_init();
    uint16_t c0 = cyc();
    delay_cycles(20000);
    fr_bench.timg_counts = (uint16_t) (cyc() - c0);
    uint32_t best = UINT32_MAX;
    for (int i = 0; i < 16; i++) {
        uint16_t a = cyc(), b = cyc();
        if ((uint16_t) (b - a) < best) best = (uint16_t) (b - a);
    }
    fr_bench.timer_read = best;
    stat_init(&fr_bench.yield_idle); stat_init(&fr_bench.isr_to_task);
    bench_handle = xTaskCreateStatic(bench_task, "bnch", configMINIMAL_STACK_SIZE, NULL, 0, bench_stack, &bench_tcb);
    vTaskStartScheduler();
    for (;;) { }
}
