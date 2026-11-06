/* Dhrystone 2.1 on the LP-EM-CC35X1 as a FreeRTOS thread in a copy of TI's drivers/uart2echo. The benchmark sources are
 * the ones in mspm0l1306/bench/dhrystone/src, unmodified (K&R C, library calls renamed with -D as on the other boards).
 * The chip's cycle counter reads 0 here, so the two time() calls stamp the 1 ms FreeRTOS tick. Report on the XDS110
 * serial port at 115200: D,<runs>,<ms>,<dhrystones per second>,<checks_bad> */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <FreeRTOS.h>
#include <task.h>
#include <ti/drivers/UART2.h>
#include "ti_drivers_config.h"
#include "dhry.h"

extern int dhry_main(void);
extern Rec_Pointer Ptr_Glob, Next_Ptr_Glob;
extern int Int_Glob, Arr_1_Glob[50], Arr_2_Glob[50][50];
extern Boolean Bool_Glob;
extern char Ch_1_Glob, Ch_2_Glob;

static volatile uint32_t t_start, t_end;
static int calls;
long time(long *t) { (void)t; if (calls++ == 0) { t_start = xTaskGetTickCount(); return 0; } t_end = xTaskGetTickCount(); return (long)((t_end - t_start) / 1000u); }
int dh_printf(const char *fmt, ...) { (void)fmt; return 0; }
int dh_scanf(const char *fmt, ...) { va_list ap; (void)fmt; va_start(ap, fmt); *va_arg(ap, int *) = (int)DH_RUNS; va_end(ap); return 1; }
static uint32_t pool[64]; static unsigned used;
char *dh_malloc(unsigned n) { char *p = (char *)pool + used; used += (n + 3u) & ~3u; return used > sizeof pool ? 0 : p; }

void *mainThread(void *arg0)
{
	(void)arg0;
	UART2_Params p; UART2_Params_init(&p); p.baudRate = 115200;
	UART2_Handle u = UART2_open(CONFIG_UART2_0, &p);
	char line[120];
	for (;;) {
		calls = 0;
		dhry_main();
		uint32_t bad = 0, ms = t_end - t_start;
		if (Int_Glob != 5) bad |= 1; if (Bool_Glob != 1) bad |= 2; if (Ch_1_Glob != 'A') bad |= 4; if (Ch_2_Glob != 'B') bad |= 8;
		if (Arr_1_Glob[8] != 7) bad |= 16; if (Arr_2_Glob[8][7] != (int)DH_RUNS + 10) bad |= 32;
		if (Ptr_Glob->Discr != 0 || Ptr_Glob->variant.var_1.Enum_Comp != 2 || Ptr_Glob->variant.var_1.Int_Comp != 17) bad |= 64;
		if (Next_Ptr_Glob->Discr != 0 || Next_Ptr_Glob->variant.var_1.Enum_Comp != 1 || Next_Ptr_Glob->variant.var_1.Int_Comp != 18) bad |= 128;
		if (strcmp(Ptr_Glob->variant.var_1.Str_Comp, "DHRYSTONE PROGRAM, SOME STRING") != 0) bad |= 256;
		int n = snprintf(line, sizeof line, "D,%lu,%lu,%lu,%lu\r\n", (unsigned long)DH_RUNS, (unsigned long)ms,
				 (unsigned long)(((uint64_t)DH_RUNS * 1000u) / (ms ? ms : 1)), (unsigned long)bad);
		if (u) UART2_write(u, line, n, NULL);
		sleep(5);
		used = 0;
	}
}
