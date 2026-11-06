/* frl_gp_check.h: the FRL-GP1 console check, portable. The board (or the host)
 * supplies four functions; this file has no device headers. */
#ifndef FRL_GP_CHECK_H
#define FRL_GP_CHECK_H
#include <stdint.h>
#include "frl_gamepad.h"

/* supplied by the glue */
extern void     frl_out(const char *s);          /* write a NUL-terminated string to the console */
extern uint32_t frl_micros(void);                /* free-running microsecond counter, wraps at 2^32 */
extern uint32_t frl_heap_free(void);
extern uint32_t frl_uptime_ms(void);

/* One-time start-up: default config, the JOBSPEC call sequence, the 200 / 1000
 * timing loops. Fills the stored block. Returns 0 when the descriptor was built. */
int  frl_gp_check_init(void);
/* Prints one FRL-GP1 block (INFO is read fresh each time). */
void frl_gp_check_print(void);
/* The demo's state, for the BLE glue: the descriptor and a report in the library's order. */
const frl_gp_t *frl_gp_check_gamepad(void);
#endif
