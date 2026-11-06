/* Stand-in for the SysConfig header the MSPM0 app includes, on the AM263Px (MCU+ SDK). Four lines of meaning:
   the MSPM0 init call becomes the MCU+ init sequence the SDK's own main.c runs; nothing else the app uses is board code. */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h
#include "ti_drivers_config.h"
#include "ti_board_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#define SYSCFG_DL_init() do { System_init(); Board_init(); Drivers_open(); Board_driversOpen(); } while (0)
#endif
