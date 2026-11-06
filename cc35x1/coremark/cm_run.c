/* CoreMark runner for the LP-EM-CC35X1: replaces uart2echo.c in a copy of TI's drivers/uart2echo example
 * (prebuild.sh does the copy). Runs EEMBC CoreMark (third-party/coremark, unmodified) once in the example's
 * thread, then prints CoreMark's report and the two clocks on the XDS110 serial port at 115200, every 10 s.
 * NOT RUN YET (written 2026-10-07). The Wi-Fi and Bluetooth stacks are not started. */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ti/drivers/UART2.h>
#include <ti/drivers/dpl/ClockP.h>
#include "ti_drivers_config.h"

extern int coremark_main(void);
extern char cm_log[];
extern unsigned cm_len;
extern volatile uint32_t cm_cycles, cm_ticks_rtos;

void *mainThread(void *arg0)
{
	(void)arg0;
	UART2_Params p;
	UART2_Params_init(&p);
	p.baudRate = 115200;
	UART2_Handle u = UART2_open(CONFIG_UART2_0, &p);
	char line[160];
	size_t n;
	ClockP_FreqHz f;
	ClockP_getCpuFreq(&f);
	n = (size_t)snprintf(line, sizeof line, "BENCH START cc35x1 coremark, ClockP cpu %lu Hz\r\n", (unsigned long)f.lo);
	if (u) UART2_write(u, line, n, NULL);
	coremark_main();
	for (;;) {
		if (u) {
			UART2_write(u, cm_log, cm_len, NULL);
			n = (size_t)snprintf(line, sizeof line, "I,cycles,%lu\r\nI,rtos_ticks,%lu\r\nBENCH DONE\r\n",
					     (unsigned long)cm_cycles, (unsigned long)cm_ticks_rtos);
			UART2_write(u, line, n, NULL);
		}
		sleep(10);
	}
}
