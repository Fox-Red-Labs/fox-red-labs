/* Host harness standing in for the MSPM0 SysConfig header: the app's one init call arms a one-second timer that
   prints the app's result globals and exits, because the app itself ends in while (1). __BKPT is the app's own
   failure path: it prints FAIL and exits 1. No line of the app changes. */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h
#include <stdint.h>
void host_init(void);
#define SYSCFG_DL_init() host_init()
#endif
