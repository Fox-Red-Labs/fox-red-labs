/*
 * prov_glue.h - Bluetooth LE Wi-Fi provisioning for the chained demo (see
 * prov_glue.c). Fox Red Labs, 2026-10-08.
 */
#ifndef PROV_GLUE_H
#define PROV_GLUE_H

#include <stdint.h>

int32_t prov_init(void);       /* once, before prov_ble_start() */
int32_t prov_ble_start(void);        /* NimBLE host up; needs Wlan_Start() first */
void    advConfigure(void);
void    advEnable(void);
void    prov_adv_stop(void);

/* Block until the phone presses Connect. ssid needs 33 bytes, pass 65.
 * secType is the byte the app wrote (WLAN_SEC_TYPE_* values). */
int  prov_wait_credentials(char *ssid, uint8_t *secType, char *pass, uint8_t *passLen);

/* Tell the phone how the join went (GATT notification). */
void prov_notify(int connected);

#endif
