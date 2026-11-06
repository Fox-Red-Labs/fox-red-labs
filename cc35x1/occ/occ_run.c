/* The four room-occupancy models (mspm0g3507/mcu-bench/edgeai/uci-room-occupancy), plain C integer code,
 * on the LP-EM-CC35X1 as a FreeRTOS thread in a copy of TI's drivers/uart2echo. Same checks as
 * cc2340r5/bench/tests/ai_occ/main_c.c: for each of the 20 test vectors the class and all four logits
 * are checked against onnxruntime's answers. The chip's cycle counter reads 0 here (DWT, see cc35x1/README.md), so
 * time comes from the 1 ms RTOS tick: each vector runs the whole model in a loop of at least 100 ms and the mean
 * per call is reported (min and max over the 20 vectors). Output lines on the XDS110 serial port at 115200:
 *   R,<model>,<min us>,<max us>,<class_ok of 20>,<bad logits> */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <FreeRTOS.h>
#include <task.h>
#include <ti/drivers/UART2.h>
#include "ti_drivers_config.h"
#include "occ_cnn_500/occ_cnn_500.h"
#include "occ_cnn_2k/occ_cnn_2k.h"
#include "occ_cnn_8k/occ_cnn_8k.h"
#include "occ_cnn_30k/occ_cnn_30k.h"
#include "occ_cnn_500/test_vectors.h"
#include "occ_cnn_2k/test_vectors.h"
#include "occ_cnn_8k/test_vectors.h"
#include "occ_cnn_30k/test_vectors.h"

typedef struct {
	const char *name;
	int (*run)(const float *, int8_t *);
	const float (*x)[128];
	const uint8_t *cls;
	const int8_t (*lg)[4];
} model_t;
#define MODEL(M) { #M, M##_run, M##_test_x, M##_test_expected_class, M##_test_expected_logits }
static const model_t models[] = { MODEL(occ_cnn_500), MODEL(occ_cnn_2k), MODEL(occ_cnn_8k), MODEL(occ_cnn_30k) };

void *mainThread(void *arg0)
{
	(void)arg0;
	UART2_Params p;
	UART2_Params_init(&p);
	p.baudRate = 115200;
	UART2_Handle u = UART2_open(CONFIG_UART2_0, &p);
	char line[200];
	for (;;) {
		int n = snprintf(line, sizeof line, "BENCH START cc35x1 occupancy\r\n");
		if (u) UART2_write(u, line, n, NULL);
		for (unsigned m = 0; m < sizeof models / sizeof models[0]; m++) {
			const model_t *M = &models[m];
			uint32_t bad = 0, ok = 0, mn = UINT32_MAX, mx = 0;
			int8_t lg[4];
			for (int v = 0; v < 20; v++) {
				int c = M->run(M->x[v], lg);
				ok += (c == M->cls[v]);
				for (int k = 0; k < 4; k++) bad += (lg[k] != M->lg[v][k]);
				uint32_t reps = 0, t0 = xTaskGetTickCount();
				while ((uint32_t)(xTaskGetTickCount() - t0) < 100) { (void)M->run(M->x[v], lg); reps++; }
				uint32_t dt = (uint32_t)(xTaskGetTickCount() - t0);   /* ms */
				uint32_t us = (uint32_t)(((uint64_t)dt * 1000u) / reps);
				if (us < mn) mn = us; if (us > mx) mx = us;
			}
			n = snprintf(line, sizeof line, "R,%s,%lu,%lu,%lu,%lu\r\n", M->name, (unsigned long)mn, (unsigned long)mx, (unsigned long)ok, (unsigned long)bad);
			if (u) UART2_write(u, line, n, NULL);
		}
		n = snprintf(line, sizeof line, "BENCH DONE\r\n");
		if (u) UART2_write(u, line, n, NULL);
		sleep(10);
	}
}
