/*
 * Bluetooth light service for the basic_ble example (Fox Red Labs).
 * Derived from TI's simple_gatt profile in the SimpleLink Low Power F3 SDK.
 * Copyright (c) 2010-2026, Texas Instruments Incorporated. All rights reserved.
 * TI's BSD 3-Clause notice is at the top of frl_light.c and applies here.
 */
#ifndef FRL_LIGHT_H
#define FRL_LIGHT_H

#include "ti/ble/app_util/framework/bleapputil_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Registers the service, sets the LEDs and starts the 1 s status notification. Call once after the stack is up. */
bStatus_t FrlLight_start(void);

#ifdef __cplusplus
}
#endif

#endif
