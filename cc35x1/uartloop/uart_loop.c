/* UART loop test for the LP-EM-CC35X1 (checkpoint W3). One jumper wire from header pin 4 (P29, GPIO17, UART0 TX)
 * to header pin 3 (P28, GPIO18, UART0 RX). For each baud rate, send 4,096 bytes of a counting pattern on UART0, read them
 * back by DMA, compare. The report goes out of UART1 (the probe's serial port, 115200):
 *   U,<baud>,<bytes sent>,<bytes received>,<byte errors> */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <FreeRTOS.h>
#include <task.h>
#include <ti/drivers/UART2.h>
#include "ti_drivers_config.h"

#define N 4096
static uint8_t txb[N], rxb[N];
static volatile uint32_t rx_done, rx_n;
static void rxcb(UART2_Handle h, void *buf, size_t n, void *arg, int_fast16_t status) { (void)h; (void)buf; (void)arg; (void)status; rx_n = (uint32_t)n; rx_done = 1; }

void *mainThread(void *arg0)
{
	(void)arg0;
	UART2_Params p; UART2_Params_init(&p); p.baudRate = 115200;
	UART2_Handle rep = UART2_open(CONFIG_UART2_0, &p);
	static const uint32_t bauds[] = { 9600, 115200, 921600, 2000000, 3000000, 4000000, 5000000, 6000000, 8000000 };
	char line[100];
	for (unsigned i = 0; i < N; i++) txb[i] = (uint8_t)(i * 7 + 3);
	for (;;) {
		int n = snprintf(line, sizeof line, "BENCH START uart loop\r\n"); if (rep) UART2_write(rep, line, n, NULL);
		for (unsigned b = 0; b < sizeof bauds / sizeof bauds[0]; b++) {
			UART2_Params q; UART2_Params_init(&q);
			q.baudRate = bauds[b]; q.readMode = UART2_Mode_CALLBACK; q.readCallback = rxcb; q.writeMode = UART2_Mode_BLOCKING;
			UART2_Handle u = UART2_open(CONFIG_UART2_1, &q);
			if (!u) { n = snprintf(line, sizeof line, "U,%lu,open_failed\r\n", (unsigned long)bauds[b]); if (rep) UART2_write(rep, line, n, NULL); continue; }
			memset(rxb, 0, sizeof rxb); rx_done = 0; rx_n = 0;
			UART2_read(u, rxb, N, NULL);
			size_t w = 0; UART2_write(u, txb, N, &w);
			uint32_t t0 = xTaskGetTickCount();
			while (!rx_done && (uint32_t)(xTaskGetTickCount() - t0) < 1500) vTaskDelay(1);
			if (!rx_done) UART2_readCancel(u);
			uint32_t err = 0; for (unsigned i = 0; i < N; i++) err += (rxb[i] != txb[i]);
			n = snprintf(line, sizeof line, "U,%lu,%lu,%lu,%lu\r\n", (unsigned long)bauds[b], (unsigned long)w, (unsigned long)rx_n, (unsigned long)err);
			if (rep) UART2_write(rep, line, n, NULL);
			UART2_close(u);
		}
		n = snprintf(line, sizeof line, "BENCH DONE\r\n"); if (rep) UART2_write(rep, line, n, NULL);
		sleep(5);
	}
}
