/*
 * Copyright (c) 2024, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Modified by Fox Red Labs, 2026. Derived from TI's demos/ble_wifi_provisioning/ble_wifi_provisioning.c and ble_cmd.c (SimpleLink Wi-Fi SDK 10.20.00.39).
 * The TI copyright and license above are kept as in the original file.
 */
/*
 * prov_glue.c - the parts of TI's ble_wifi_provisioning demo that the chained
 * image needs, without its network-terminal command shell.
 *
 * TI's nimble_host_provisioning.c (the GATT service the phone app writes to) is
 * compiled unmodified from the SDK. It calls back into provSaveSSID(),
 * provSavePassword(), provSaveSecurity(), advEnable() and signals
 * prov_CB.startConnectEventSyncObj; those live here.
 *
 * Copied from TI's examples/rtos/LP_EM_CC35X1/demos/ble_wifi_provisioning/
 * ble_wifi_provisioning.c (SimpleLink Wi-Fi SDK 10.20.00.39, BSD-3-Clause,
 * Copyright (c) Texas Instruments Incorporated): prov_CB, provSaveSSID,
 * provSavePassword, provSaveSecurity, provClearCredentials, advConfigure,
 * advEnable, and initProvVariables (as prov_init). prov_ble_start() is TI's
 * cmdBleStartCallback() from ble_cmd.c without the network-terminal check
 * (renamed, since TI's header declares bleStart() as void).
 * New here: prov_wait_credentials(), prov_notify(), prov_adv_stop().
 * Fox Red Labs, 2026-10-08.
 */
#include <string.h>

#include "ble_wifi_provisioning.h"   /* TI's header, unmodified */
#include "nimble_host_provisioning.h"
#include "ble_if.h"
#include "uart_term.h"

#include "prov_glue.h"

#define WLAN_SEC_TYPE_UNDEFINED 0xFF

provControlBlock prov_CB;

/* ---- copied from TI's ble_wifi_provisioning.c ---- */

void provSaveSSID(uint8_t *data, uint8_t length)
{
    if (data == NULL || length == 0)
    {
        Report("\r\n[BLE GATT]: ERROR - Invalid SSID data");
        return;
    }
    if (length > WLAN_SSID_MAX_LENGTH)
    {
        length = WLAN_SSID_MAX_LENGTH;
    }
    osi_LockObjLock(&prov_CB.credentialsMutex, OSI_WAIT_FOREVER);
    os_memset(prov_CB.wlanSSID, 0, WLAN_SSID_MAX_LENGTH);
    os_memcpy(prov_CB.wlanSSID, data, length);
    prov_CB.wlanPasswordSet = FALSE;
    osi_LockObjUnlock(&prov_CB.credentialsMutex);
    Report("\r\n[BLE GATT]: WLAN SSID Received");
}

void provSavePassword(uint8_t *data, uint8_t length)
{
    if (data == NULL || length == 0)
    {
        Report("\r\n[BLE GATT]: ERROR - Invalid password data");
        return;
    }
    if (length > PASSWD_LEN_MAX)
    {
        length = PASSWD_LEN_MAX;
    }
    osi_LockObjLock(&prov_CB.credentialsMutex, OSI_WAIT_FOREVER);
    os_memset(prov_CB.wlanPASSWORD, 0, PASSWD_LEN_MAX);
    os_memcpy(prov_CB.wlanPASSWORD, data, length);
    prov_CB.wlanPasswordLength = length;
    prov_CB.wlanPasswordSet = TRUE;
    osi_LockObjUnlock(&prov_CB.credentialsMutex);
    Report("\r\n[BLE GATT]: WLAN Password Received");
}

void provSaveSecurity(uint8_t *data, uint8_t length)
{
    if (data == NULL || length < 1)
    {
        Report("\r\n[BLE GATT]: ERROR - Invalid security data");
        return;
    }
    osi_LockObjLock(&prov_CB.credentialsMutex, OSI_WAIT_FOREVER);
    prov_CB.wlanSecurity = data[0];
    osi_LockObjUnlock(&prov_CB.credentialsMutex);
    Report("\r\n[BLE GATT]: WLAN Security Type Received");
}

void provClearCredentials(void)
{
    osi_LockObjLock(&prov_CB.credentialsMutex, OSI_WAIT_FOREVER);
    os_memset(prov_CB.wlanSSID, 0, WLAN_SSID_MAX_LENGTH);
    os_memset(prov_CB.wlanPASSWORD, 0, PASSWD_LEN_MAX);
    prov_CB.wlanPasswordLength = 0;
    prov_CB.wlanPasswordSet = FALSE;
    prov_CB.wlanSecurity = WLAN_SEC_TYPE_UNDEFINED;
    osi_LockObjUnlock(&prov_CB.credentialsMutex);
}

void advConfigure(void)
{
    ExtAdvCfg_t extAdvParams;

    extAdvParams.instance = 0;
    extAdvParams.legacy = 1;
    extAdvParams.interval_ms = 100;
    extAdvParams.prim_phy = 0x1;
    extAdvParams.sec_phy = 0x1;

    nimble_host_ext_adv_cfg(&extAdvParams);
}

void advEnable(void)
{
    ExtAdvEnable_t extAdvEnable;

    extAdvEnable.enable = 1;
    extAdvEnable.instance = 0;
    extAdvEnable.duration = 0;
    extAdvEnable.max_events = 0;

    nimble_host_ext_adv_enable(&extAdvEnable);
}

/* TI's initProvVariables() */
int32_t prov_init(void)
{
    if (osi_LockObjCreate(&prov_CB.credentialsMutex) != 0)
    {
        return -1;
    }
    provClearCredentials();
    if (osi_SyncObjCreate(&prov_CB.startConnectEventSyncObj) != 0)
    {
        return -1;
    }
    return 0;
}

/* TI's cmdBleStartCallback() (ble_cmd.c), minus the network-terminal "is the
 * interface up" check: the caller has already run Wlan_Start(). */
int32_t prov_ble_start(void)
{
    int ret = 0;

    BleIf_OpenTransport();
    if (!nimble_host_is_enabled())
    {
        ret = nimble_host_start();
        if (nimble_host_is_enabled() && ret == 0)
        {
            Report("\n\rBLE start success!\n\r");
        }
        else
        {
            Report("\n\rBLE failed to initialize, error: %d\n\r", ret);
            if (ret == 0)
            {
                ret = -1;
            }
        }
    }
    else
    {
        Report("\n\rBLE is already running\n\r");
    }
    return ret;
}

/* ---- new for the chained demo ---- */

int prov_wait_credentials(char *ssid, uint8_t *secType, char *pass, uint8_t *passLen)
{
    while (1)
    {
        if (osi_SyncObjWait(&prov_CB.startConnectEventSyncObj, OSI_WAIT_FOREVER) != OSI_OK)
        {
            continue;
        }
        osi_LockObjLock(&prov_CB.credentialsMutex, OSI_WAIT_FOREVER);
        memset(ssid, 0, WLAN_SSID_MAX_LENGTH + 1);
        memcpy(ssid, prov_CB.wlanSSID, WLAN_SSID_MAX_LENGTH);
        *secType = prov_CB.wlanSecurity;
        *passLen = prov_CB.wlanPasswordSet ? prov_CB.wlanPasswordLength : 0;
        memset(pass, 0, PASSWD_LEN_MAX + 1);
        memcpy(pass, prov_CB.wlanPASSWORD, *passLen);
        osi_LockObjUnlock(&prov_CB.credentialsMutex);
        provClearCredentials();

        if (*secType == WLAN_SEC_TYPE_UNDEFINED)
        {
            Report("\r\n[PROV] security type not set; send it from the app and press Connect again\r\n");
            nimble_host_gatt_svr_chr_notify_wlan_connection(WLAN_CONNECT_FAILURE);
            continue;
        }
        nimble_host_gatt_svr_chr_notify_wlan_connection(WLAN_CONNECT_START);
        return 0;
    }
}

void prov_notify(int connected)
{
    nimble_host_gatt_svr_chr_notify_wlan_connection(connected ? WLAN_CONNECT_SUCCESS : WLAN_CONNECT_FAILURE);
}

void prov_adv_stop(void)
{
    ExtAdvEnable_t extAdvEnable;

    extAdvEnable.enable = 0;
    extAdvEnable.instance = 0;
    extAdvEnable.duration = 0;
    extAdvEnable.max_events = 0;
    nimble_host_ext_adv_enable(&extAdvEnable);
}
