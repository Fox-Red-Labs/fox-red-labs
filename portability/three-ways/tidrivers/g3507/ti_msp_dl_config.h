/* Three ways, TI Drivers: the DriverLib part of the board setup (power, clocks, VREF). Hand-written C. */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>
#define POWER_STARTUP_DELAY (16)
void SYSCFG_DL_init(void);
#endif
