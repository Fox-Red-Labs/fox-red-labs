/* Stand-in for the SysConfig header an MSPM0 app includes. The AM13E2x SDK generates ti_sdk_dl_config.h instead;
   this one pulls it in and adds the two MSPM0 names the app code uses that the AM13 driverlib does not provide. */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h
#include "ti_sdk_dl_config.h"
#define CPUCLK_FREQ  CPUCLK_FREQ_HZ
#define delay_cycles DL_Common_delayCycles
/* MSPM0 SysConfig names one port per GPIO group; the AM13 generator names one per pin. Pins of the group sit on one port. */
#ifdef GPIO_LEDS_USER_LED_1_PORT
#define GPIO_LEDS_PORT GPIO_LEDS_USER_LED_1_PORT
#endif
/* Pin and instance names for peripherals the AM13 generator cannot emit, one small header per program. */
#if __has_include("board_pins.h")
#include "board_pins.h"
#endif
#endif
