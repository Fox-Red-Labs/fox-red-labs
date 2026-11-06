/* Fox Red Labs: glue between TI's CSI presence pipeline (EdgeAI plugin 1.20 example, unmodified files in csi/ti/)
 * and the Matter thermostat on the LP-EM-CC35X1. C linkage; called from C and C++. */
#ifndef CSI_GLUE_H
#define CSI_GLUE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct pres_result;              /* csi_presence.h: pres_result_t */
/* Start the watcher task: waits for the station to be connected, then enables CSI in the firmware, sets the
 * solicitation toward the joined access point, and starts the reader thread. Safe to call once at boot. */
void csi_glue_start(void);
/* Called by csi_presence.c (patched) after every window with the result and the compute time in milliseconds. */
void csi_glue_result(const void *pres_result, uint32_t compute_ms);
/* Free-running 32-bit cycle counter (DWT) and the millisecond conversion, for the per-window timing line. */
uint32_t csi_glue_cycles(void);
uint32_t csi_glue_ms_since(uint32_t cycles_start);
/* Implemented in ThermostatManager.cpp: the result reaches the Occupancy Sensing endpoint and the log. */
void ThermoCsiResult(int decision, float p1, float pavg, float pps, uint32_t win_index, uint32_t compute_ms);
int Report(const char *fmt, ...);
#ifdef __cplusplus
}
#endif
#endif
