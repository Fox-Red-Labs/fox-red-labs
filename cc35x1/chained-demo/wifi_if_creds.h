/*
 * wifi_if_creds.h - prototypes for the two functions added to the copy of TI's
 * wifi_if.c in this folder (TI's wifi_if.h is used unmodified).
 * Fox Red Labs, 2026-10-08.
 */
#ifndef WIFI_IF_CREDS_H
#define WIFI_IF_CREDS_H

#include <stdint.h>
#include "wifi_if.h"

/* ssid NULL clears. secType is a WLAN_SEC_TYPE_* code from wlan_if.h. */
int WIFI_IF_setCredentials(const char *ssid, uint8_t secType, const char *pass, uint8_t passLen);

/* Join with the credentials set above, on a handle WIFI_IF_start() returned. */
int WIFI_IF_connectCredentials(void *hConnReq, WifiServiceLevel_e level, unsigned long timeout_ms);

#endif
