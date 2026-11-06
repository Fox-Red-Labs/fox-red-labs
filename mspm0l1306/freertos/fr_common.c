/*
 * Shared by every FreeRTOS image for the LP-MSPM0L1306: LED1 setup and fault recording.
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
    /* LED1 (red) on PA0, active low: low = on. Same pin TI's L1306 examples call USER_LED_1. */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM1);
    DL_GPIO_setPins(GPIOA, DL_GPIO_PIN_0);
    DL_GPIO_enableOutput(GPIOA, DL_GPIO_PIN_0);
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
