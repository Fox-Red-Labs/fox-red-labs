/*
 * hid_remote.h: a Bluetooth LE media remote (HID over GATT, consumer control)
 * for TI's BLE5-Stack, written for the LP-EM-CC2340R5 on top of TI's basic_ble
 * example. Fox Red Labs, ports/ble-hid-remote. Not a TI file.
 */
#ifndef HID_REMOTE_H
#define HID_REMOTE_H

#include <stdint.h>
#include "ti/ble/app_util/framework/bleapputil_api.h"

/* Bits of the one-byte input report (report ID 1). Order matches the usages
 * in the report map in hid_remote.c; host/report_map.py checks that it does. */
#define HID_CC_PLAY_PAUSE   (1u << 0)   /* usage 0xCD Play/Pause          */
#define HID_CC_NEXT         (1u << 1)   /* usage 0xB5 Scan Next Track     */
#define HID_CC_PREVIOUS     (1u << 2)   /* usage 0xB6 Scan Previous Track */
#define HID_CC_VOLUME_UP    (1u << 3)   /* usage 0xE9 Volume Increment    */
#define HID_CC_VOLUME_DOWN  (1u << 4)   /* usage 0xEA Volume Decrement    */

#define HID_REPORT_ID_CC    1           /* report ID in the map and in the Report Reference */
#define HID_REPORT_TYPE_IN  1           /* Report Reference type: input */

/* Adds the Battery and HID services, sets the GAP appearance and opens both
 * LaunchPad buttons: left (CONFIG_BUTTON_0) is play/pause, right
 * (CONFIG_BUTTON_1) is next track. Call once from the stack-init-done handler. */
bStatus_t HidRemote_start(void);

#endif /* HID_REMOTE_H */
