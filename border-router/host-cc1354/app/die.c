/*
 * When the host gives up, say so on the console before stopping.
 *
 * OpenThread's DieNow() ends in exit(); newlib's assert() ends in __assert_func(); FreeRTOS reports a blown task
 * stack through vApplicationStackOverflowHook(); ot-ti's configASSERT spins. On a board with no debugger attached
 * every one of those looks the same from outside: the CLI goes quiet. Each now prints one line through the console
 * UART (blocking write, interrupts off) and then parks the CPU, so the reason survives until someone reads it.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <openthread/instance.h>
#include <FreeRTOS.h>
#include <task.h>

#include <ti/drivers/Watchdog.h>
#include <ti/drivers/dpl/ClockP.h>
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/uart.h)
#include DeviceFamily_constructPath(inc/hw_memmap.h)
#include DeviceFamily_constructPath(inc/hw_types.h)
#include DeviceFamily_constructPath(inc/hw_uart.h)
#include DeviceFamily_constructPath(inc/hw_ints.h)
#include DeviceFamily_constructPath(driverlib/interrupt.h)

#include "ti_drivers_config.h"
#include "system.h"

void frl_die(const char *what, const char *file, int line);
void frl_install_fault_handlers(void);
void frl_watchdog_start(void);
void frl_watchdog_kick(void);
void frl_register_cli(otInstance *aInstance);
extern volatile uint32_t frl_where;
extern volatile uint32_t frl_rx_callbacks;
extern volatile uint32_t frl_console_dropped, frl_console_read_errors, frl_console_rearm_failures;
extern volatile int_fast16_t frl_console_rearm_status;
extern volatile uint32_t frl_console_tx_stuck;
extern StaticTask_t      appTaskBuffer; /* the OpenThread task, freertos_main.c */

/* The console UART is already open by ot-ti's uart.c (UART2_open on an open instance returns NULL), so this
 * goes under the driver: polled writes straight to the UART0 registers, which the XDS110 UART pins sit on. */
static void say(const char *text)
{
    HWREG(UART0_BASE + UART_O_CTL) |= UART_CTL_TXE | UART_CTL_UARTEN; /* TI's driver leaves TXE off until it writes */
    while (*text != '\0')
    {
        uint32_t spins = 200000; /* a stuck transmit FIFO must not hang the reporter too */
        while ((HWREG(UART0_BASE + UART_O_FR) & UART_FR_TXFF) && --spins)
        {
        }
        if (spins == 0)
        {
            return;
        }
        HWREG(UART0_BASE + UART_O_DR) = (uint8_t)*text++;
    }
}

void frl_die(const char *what, const char *file, int line)
{
    char        buf[160];
    const char *slash = file ? strrchr(file, '/') : NULL;

    taskDISABLE_INTERRUPTS();
    snprintf(buf, sizeof(buf), "\r\nFRL DIE: %s at %s:%d\r\n", what, slash ? slash + 1 : (file ? file : "?"), line);
    say(buf);

    for (;;)
    {
    }
}

/* newlib: exit() -> _exit(); OpenThread's DieNow(code) lands here with its otExitCode */
void _exit(int code)
{
    char buf[64];

    taskDISABLE_INTERRUPTS();
    snprintf(buf, sizeof(buf), "\r\nFRL DIE: exit(%d)\r\n", code);
    say(buf);

    for (;;)
    {
    }
}

/* newlib: assert() */
void __assert_func(const char *file, int line, const char *func, const char *expr)
{
    (void)func;
    frl_die(expr ? expr : "assert", file, line);
}

/* FreeRTOS, configCHECK_FOR_STACK_OVERFLOW 2 */
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    frl_die("stack overflow", name ? name : "task", 0);
}

/* ---- CPU faults -----------------------------------------------------------------------------------------------
 * TI's startup file parks every fault in a silent static loop. These handlers are installed over it at run time
 * (driverlib IntRegister moves the vector table to RAM) and print the fault status registers, the stacked frame
 * and the current task before parking, so a hard fault looks different from a sleep or a deadlock. */
void fault_report(uint32_t *frame, uint32_t exc_return) __attribute__((used, noreturn));
void fault_report(uint32_t *frame, uint32_t exc_return)
{
    char        buf[200];
    const char *task = "none";

    taskDISABLE_INTERRUPTS();
    if (xTaskGetCurrentTaskHandle() != NULL)
    {
        task = pcTaskGetName(NULL);
    }
    snprintf(buf, sizeof(buf), "\r\nFRL FAULT: icsr=%08lx cfsr=%08lx hfsr=%08lx mmfar=%08lx bfar=%08lx\r\n",
             *(volatile uint32_t *)0xE000ED04, *(volatile uint32_t *)0xE000ED28, *(volatile uint32_t *)0xE000ED2C,
             *(volatile uint32_t *)0xE000ED34, *(volatile uint32_t *)0xE000ED38);
    say(buf);
    snprintf(buf, sizeof(buf), "FRL FAULT: pc=%08lx lr=%08lx xpsr=%08lx r0=%08lx r1=%08lx r2=%08lx r3=%08lx r12=%08lx sp=%08lx exc=%08lx task=%s\r\n",
             frame[6], frame[5], frame[7], frame[0], frame[1], frame[2], frame[3], frame[4], (uint32_t)frame,
             exc_return, task);
    say(buf);

    /* Poor man's backtrace: every word from the faulting SP up to the top of the OpenThread task's stack that
     * looks like a Thumb code address. Feed them to arm-none-eabi-addr2line. */
    {
        extern StackType_t appStack[];
        uint32_t *top   = (uint32_t *)((uint8_t *)appStack + 8192 * sizeof(StackType_t));
        uint32_t *p     = frame;
        int       count = 0;
        say("FRL STACK:");
        for (; p < top && count < 400; p++)
        {
            uint32_t w = *p;
            if (w > 0x100 && w < 0x80000 && (w & 1))
            {
                snprintf(buf, sizeof(buf), " %lx", w);
                say(buf);
                count++;
            }
        }
        snprintf(buf, sizeof(buf), "\r\nFRL STACK END: %d candidates, %lu bytes from sp to top\r\n", count, (uint32_t)((uint8_t *)top - (uint8_t *)frame));
        say(buf);
    }

    for (;;)
    {
    }
}

__attribute__((naked)) static void fault_entry(void)
{
    __asm volatile("tst lr, #4\n"
                   "ite eq\n"
                   "mrseq r0, msp\n"
                   "mrsne r0, psp\n"
                   "mov r1, lr\n"
                   "b fault_report\n");
}

void frl_install_fault_handlers(void)
{
    IntRegister(INT_HARD_FAULT, fault_entry);
    IntRegister(INT_MEMMANAGE_FAULT, fault_entry);
    IntRegister(INT_BUS_FAULT, fault_entry);
    IntRegister(INT_USAGE_FAULT, fault_entry);
    /* let the three configurable faults report as themselves instead of escalating */
    *(volatile uint32_t *)0xE000ED24 |= (1u << 16) | (1u << 17) | (1u << 18);
}

/* ---- Watchdog as a hang reporter -------------------------------------------------------------------------------
 * The OpenThread task kicks the watchdog from its radio process loop, which a 500 ms clock keeps triggering. If the
 * task stops running for 3 s (spinning, or blocked on something that never comes) the watchdog interrupt prints
 * who is running, where the OpenThread task last was (breadcrumb and its saved program counter) and then lets
 * the second timeout reset the chip, so the bench comes back on its own. If interrupts are masked the interrupt
 * cannot print and the reset alone says so. */
static Watchdog_Handle sWatchdog;
static ClockP_Struct   sKickClockStruct;

static void watchdog_fired(uintptr_t arg)
{
    char           buf[220];
    uint32_t      *tcb = (uint32_t *)&appTaskBuffer;
    uint32_t      *top = (uint32_t *)tcb[0];           /* pxTopOfStack of the OpenThread task */
    uint32_t       exc = top[8];                       /* EXC_RETURN saved by the Cortex-M33 port */
    uint32_t       pc  = (exc & 0x10) ? top[15] : top[31]; /* hardware frame after r4-r11 (+ s16-s31 when FPU used) */
    uint32_t       lr  = (exc & 0x10) ? top[14] : top[30];
    uint32_t       psp;
    const char    *cur = "none";

    (void)arg;
    __asm volatile("mrs %0, psp" : "=r"(psp));
    if (xTaskGetCurrentTaskHandle() != NULL)
    {
        cur = pcTaskGetName(NULL);
    }
    snprintf(buf, sizeof(buf), "\r\nFRL WDT: OpenThread task stalled 3 s. running=%s where=%02lx rxcb=%lu ot-task saved pc=%08lx lr=%08lx exc=%08lx cur-psp-pc=%08lx icsr=%08lx\r\n",
             cur, frl_where, frl_rx_callbacks, pc, lr, exc, ((uint32_t *)psp)[6], *(volatile uint32_t *)0xE000ED04);
    say(buf);
    snprintf(buf, sizeof(buf), "FRL WDT: console dropped=%lu readerr=%lu rearmfail=%lu rearmstatus=%d txstuck=%lu uart0fr=%08lx uart1fr=%08lx\r\n", frl_console_dropped, frl_console_read_errors, frl_console_rearm_failures, (int)frl_console_rearm_status, frl_console_tx_stuck, HWREG(UART0_BASE + UART_O_FR), HWREG(UART1_BASE + UART_O_FR));
    say(buf);
    say("FRL WDT: resetting\r\n");
}

static void kick_clock(uintptr_t arg)
{
    (void)arg;
    platformRadioSignal(2); /* wakes the OpenThread process loop; platformRadioProcess calls frl_watchdog_kick */
}

void frl_watchdog_kick(void)
{
    if (sWatchdog != NULL)
    {
        Watchdog_clear(sWatchdog);
    }
}

void frl_watchdog_start(void)
{
    Watchdog_Params params;
    ClockP_Params   cp;

    Watchdog_init();
    Watchdog_Params_init(&params);
    params.callbackFxn    = watchdog_fired;
    params.resetMode      = Watchdog_RESET_OFF; /* report only: a reset leaves a debugger-loaded program halted on this board */
    params.debugStallMode = Watchdog_DEBUG_STALL_ON;
    sWatchdog             = Watchdog_open(CONFIG_WATCHDOG0, &params);
    if (sWatchdog != NULL)
    {
        Watchdog_setReload(sWatchdog, Watchdog_convertMsToTicks(sWatchdog, 3000));
    }
    else
    {
        say("\r\nFRL: watchdog did not open\r\n");
    }
    ClockP_Params_init(&cp);
    cp.period    = 500000 / ClockP_getSystemTickPeriod();
    cp.startFlag = true;
    ClockP_construct(&sKickClockStruct, kick_clock, cp.period, &cp);
}

/* ---- `frl` CLI commands: status, hang (watchdog test), fault (fault handler test) ------------------------------ */
#include <openthread/cli.h>

static otError frl_cmd(void *aContext, uint8_t aArgsLength, char *aArgs[])
{
    (void)aContext;
    if (aArgsLength >= 1 && strcmp(aArgs[0], "hang") == 0)
    {
        otCliOutputFormat("hanging the OpenThread task on purpose; expect FRL WDT in 3 s (report only, no reset)\r\n");
        taskDISABLE_INTERRUPTS();
        for (;;)
        {
        }
    }
    if (aArgsLength >= 1 && strcmp(aArgs[0], "spin") == 0)
    {
        otCliOutputFormat("spinning the OpenThread task with interrupts on; expect FRL WDT in 3 s\r\n");
        for (;;)
        {
        }
    }
    if (aArgsLength >= 1 && strcmp(aArgs[0], "fault") == 0)
    {
        otCliOutputFormat("reading address 0xCCCCCCCC on purpose; expect FRL FAULT\r\n");
        return (otError)(*(volatile uint32_t *)0xCCCCCCCCu);
    }
    otCliOutputFormat("stack free=%lu words of %u; where=%02lx rxcb=%lu console dropped=%lu readerr=%lu rearmfail=%lu rearmstatus=%d txstuck=%lu uart0fr=%08lx uart1fr=%08lx wdt=%s\r\n",
                      (unsigned long)uxTaskGetStackHighWaterMark(NULL), (unsigned)8192, frl_where, frl_rx_callbacks, frl_console_dropped, frl_console_read_errors, frl_console_rearm_failures,
                      (int)frl_console_rearm_status, frl_console_tx_stuck, HWREG(UART0_BASE + UART_O_FR),
                      HWREG(UART1_BASE + UART_O_FR), sWatchdog ? "armed" : "off");
    return OT_ERROR_NONE;
}

static const otCliCommand sFrlCommands[] = {{"frl", frl_cmd}};

void frl_register_cli(otInstance *aInstance) { otCliSetUserCommands(sFrlCommands, 1, aInstance); }
