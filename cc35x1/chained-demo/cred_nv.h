/*
 * cred_nv.h - keep the Wi-Fi credential record (portability/iot-chain/core/
 * cred_record.h) in the CC35X1's NV storage, through TI's NVOCMP driver.
 * Fox Red Labs, 2026-10-08.
 */
#ifndef CRED_NV_H
#define CRED_NV_H

#include "cred_record.h"

int cred_nv_load(cred_t *c);        /* CRED_OK, a CRED_E_* code, or -100 - NV error */
int cred_nv_save(const cred_t *c);  /* 0 or negative */
int cred_nv_erase(void);            /* 0 or negative */

/* Map between the record's vendor-neutral security type and WLAN_SEC_TYPE_*. */
int cred_sec_to_wlan(uint8_t sec);  /* -1 if unknown */
int cred_sec_from_wlan(int wlanSec);/* -1 if unknown */

#endif
