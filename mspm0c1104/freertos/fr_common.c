/*
 * Shared by every FreeRTOS image for the LP-MSPM0C1104: LED1 setup and fault recording.
 * A failure never prints (no UART); it leaves a code in fr_fault and stops, so the debugger can
 * read why. fr_fault.code: 0 = no fault, 0xDEAD0001 = stack overflow (task name in .task),
 * 0xDEAD0002 = configASSERT failed (kernel source line in .line), 0xDEAD0003 = hard fault
 * (faulting address in .pc).
 */
#include "ti_msp_dl_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "fr_common.h"

volatile struct {
    uint32_t code;
    uint32_t line;
    uint32_t pc;
    char task[configMAX_TASK_NAME_LEN];
} fr_fault;

void fr_led1_init(void)
{
    /* LED1 (red) on PA22, the pin TI calls GPIO_LEDS_USER_LED_1 in gpio_toggle_output. */
    DL_GPIO_initDigitalOutput(GPIO_LEDS_USER_LED_1_IOMUX);
    DL_GPIO_clearPins(GPIOA, GPIO_LEDS_USER_LED_1_PIN);
    DL_GPIO_enableOutput(GPIOA, GPIO_LEDS_USER_LED_1_PIN);
}

/* The MSPM0C1104 has no working SysTick, so FreeRTOS's tick comes from TIMG14: 16 bits,
 * clocked from BUSCLK (24 MHz), counting down from 23,999 once per millisecond. Its handler calls the port's
 * SysTick_Handler, which is FreeRTOS's tick function. The port calls vPortSetupTimerInterrupt (weak) once. */
void vPortSetupTimerInterrupt(void)
{
    static const DL_TimerG_ClockConfig clk = {
        .clockSel = DL_TIMER_CLOCK_BUSCLK, .divideRatio = DL_TIMER_CLOCK_DIVIDE_1, .prescale = 0};
    static const DL_TimerG_TimerConfig tim = {
        .timerMode = DL_TIMER_TIMER_MODE_PERIODIC, .period = (configCPU_CLOCK_HZ / configTICK_RATE_HZ) - 1U,
        .startTimer = DL_TIMER_STOP};
    DL_TimerG_reset(TIMG14);
    DL_TimerG_enablePower(TIMG14);
    delay_cycles(POWER_STARTUP_DELAY);
    DL_TimerG_setClockConfig(TIMG14, (DL_TimerG_ClockConfig *) &clk);
    DL_TimerG_initTimerMode(TIMG14, (DL_TimerG_TimerConfig *) &tim);
    DL_TimerG_enableInterrupt(TIMG14, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    NVIC_SetPriority(TIMG14_INT_IRQn, 3);
    NVIC_EnableIRQ(TIMG14_INT_IRQn);
    DL_TimerG_startCounter(TIMG14);
}

extern void SysTick_Handler(void);
void TIMG14_IRQHandler(void)
{
    if (TIMG14->CPU_INT.IIDX == DL_TIMERG_IIDX_ZERO) {
        SysTick_Handler();
    }
}

#if FR_SAFETY
void fr_assert_failed(unsigned int line)
{
    __disable_irq();
    fr_fault.line = line;
    fr_fault.code = 0xDEAD0002u;
    for (;;) {
    }
}
#endif

#if (configCHECK_FOR_STACK_OVERFLOW > 0)
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void) task;
    __disable_irq();
    for (int i = 0; i < configMAX_TASK_NAME_LEN; i++) {
        fr_fault.task[i] = name[i];
    }
    fr_fault.code = 0xDEAD0001u;
    for (;;) {
    }
}
#endif

void fr_hardfault_c(uint32_t *frame)
{
    fr_fault.pc = frame[6]; /* stacked PC */
    fr_fault.code = 0xDEAD0003u;
    for (;;) {
    }
}

/* Find the stack the fault frame went to (task stack or interrupt stack) and pass it on. */
__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile(
        " movs r0, #4          \n"
        " mov  r1, lr          \n"
        " tst  r0, r1          \n"
        " beq  1f              \n"
        " mrs  r0, psp         \n"
        " bl   fr_hardfault_c  \n"
        "1:                    \n"
        " mrs  r0, msp         \n"
        " bl   fr_hardfault_c  \n");
}
