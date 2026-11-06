/*
 * ota_glue.h - MQTT-triggered over-the-air update for the chained demo (see
 * ota_glue.c). Fox Red Labs, 2026-10-08.
 */
#ifndef OTA_GLUE_H
#define OTA_GLUE_H

#include "wlan_if.h"

typedef void (*ota_status_cb)(void *ctx, const char *msg);

int  ota_glue_boot(void);                  /* PSA FWU init and pending-state scan; 1 if a trial image is running */
int  ota_glue_in_trial(void);
int  ota_glue_select_wifi_fw_slot(void);   /* before Wlan_Start() */
void ota_glue_set_clock(void);             /* before any TLS connection */
int  ota_glue_accept(void);                /* reboots on success */
int  ota_glue_reject(void);                /* reboots on success */
int  ota_glue_update(const char *serverUrl, ota_status_cb status, void *ctx); /* reboots on success */

#endif
