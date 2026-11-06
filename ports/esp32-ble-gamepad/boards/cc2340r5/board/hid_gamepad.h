/* hid_gamepad.h: HID over GATT glue for the FRL-GP1 gamepad on TI's BLE5-Stack (LP-EM-CC2340R5).
 * Thin board file; the gamepad itself is core/frl_gamepad.c. */
#ifndef HID_GAMEPAD_H
#define HID_GAMEPAD_H
#include <stdint.h>
#include "ti/ble/app_util/framework/bleapputil_api.h"
/* Runs the start-up check, adds Battery + HID services, sets Device Information,
 * starts the 3 s console block and the 500 ms press/release loop. Call once after DevInfo_start(). */
bStatus_t HidGamepad_start(void);
#endif
