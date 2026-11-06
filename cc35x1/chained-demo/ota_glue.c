/*
 * Copyright (c) 2025, Texas Instruments Incorporated
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
 * Modified by Fox Red Labs, 2026. Derived from TI's demos/ota_example/ota_example.c and ota_wifi.c (SimpleLink Wi-Fi SDK 10.20.00.39).
 * The TI copyright and license above are kept as in the original file.
 */
/*
 * ota_glue.c - over-the-air update started by an MQTT message, built on TI's
 * ota_example modules (ota_server.c, ota_fwu.c, https_client.c and
 * network_mbedtls.c, compiled unmodified from the SDK).
 *
 * TI's ota_example.c drives the same calls from a serial menu. This file makes
 * the choices that menu asks a person for:
 *   - boot: TRIAL waits for "ota accept" or "ota reject" over MQTT (TI's trial
 *     accept step kept), STAGED after an unexpected reboot is rejected and
 *     cleaned (TI does the same), CANDIDATE left over is cancelled and cleaned
 *     (TI asks);
 *   - "ota <url>": query the server, pick the vendor image entry (slots 4 and 5),
 *     download, install, reboot.
 * Lines marked "from TI" follow ota_example.c and ota_wifi.c (SimpleLink Wi-Fi
 * SDK 10.20.00.39, BSD-3-Clause, Copyright (c) Texas Instruments Incorporated).
 * Fox Red Labs, 2026-10-08.
 */
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "osi_kernel.h"
#include "uart_term.h"

#include "ti/utils/FWU/psa_fwu.h"
#include "ota_fwu.h"
#include "ota_server.h"

#define MBEDTLS_CONFIG_FILE "config-hsm.h"
#include <mbedtls/platform.h>

#include "ota_glue.h"

extern void initialize_mbedtls_threading(void);   /* TI's network_mbedtls.c */

/* from TI: the CC35xx has no RTC; TLS date checks need a date inside the
 * server certificate's validity. Same value as TI's ota_example.c. */
#define OTA_SYSTEM_TIME_EPOCH   1767225600UL  /* Jan 1, 2026 00:00:00 UTC */

static int s_trial;

int ota_glue_boot(void)
{
    int trialFound = 0, stagedFound = 0, candidateFound = 0;

    /* from TI: mbedTLS threading and thread-safe allocators, then PSA FWU */
    initialize_mbedtls_threading();
    mbedtls_platform_set_calloc_free(os_calloc, os_free);
    psa_fwu_init();
    UART_PRINT("[OTA] PSA FWU initialized\n\r");

    OTA_FWU_scanPendingStates(&trialFound, &stagedFound, &candidateFound);

    if (trialFound > 0)
    {
        s_trial = 1;
        UART_PRINT("[OTA] TRIAL image running. Waiting for 'ota accept' or 'ota reject' over MQTT\n\r");
    }
    else if (stagedFound > 0)
    {
        /* from TI: STAGED after an unexpected reboot cannot be applied */
        UART_PRINT("[OTA] STAGED updates found after unexpected reboot; rejecting and cleaning\n\r");
        OTA_FWU_rejectAndCleanAll();
    }
    else if (candidateFound > 0)
    {
        UART_PRINT("[OTA] CANDIDATE downloads left from an interrupted update; cancelling\n\r");
        OTA_FWU_cancelAndCleanCandidates();
    }
    return s_trial;
}

int ota_glue_in_trial(void)
{
    return s_trial;
}

void ota_glue_set_clock(void)
{
    osi_SetDateTimeS(OTA_SYSTEM_TIME_EPOCH);
}

int ota_glue_select_wifi_fw_slot(void)
{
    /* from TI ota_wifi.c: tell the Wi-Fi driver which wireless firmware slot is
     * primary, before Wlan_Start() */
    psa_fwu_component_info_t wfwInfo;
    WlanConnectivityFWSlot_t fwSlot;

    fwSlot.Connectivityslot = 1;
    if (psa_fwu_query(WSOC_OR_RFTool_Slot_2, &wfwInfo) == PSA_SUCCESS && wfwInfo.impl.Primary)
    {
        fwSlot.Connectivityslot = 2;
    }
    return Wlan_Set(WLAN_SET_PRIMARY_CONNECTIVITY_FW, &fwSlot);
}

int ota_glue_accept(void)
{
    int ret;

    if (!s_trial)
    {
        return -1;
    }
    /* from TI */
    ret = psa_fwu_accept();
    if (ret == PSA_SUCCESS_REBOOT || ret == PSA_SUCCESS)
    {
        UART_PRINT("[OTA] Accepted; rebooting to commit\n\r");
        psa_fwu_request_reboot();
    }
    UART_PRINT("[OTA] Accept failed: %d\n\r", ret);
    return ret;
}

int ota_glue_reject(void)
{
    int ret;

    if (!s_trial)
    {
        return -1;
    }
    /* from TI */
    ret = psa_fwu_reject(PSA_ERROR_NOT_PERMITTED);
    if (ret == PSA_SUCCESS_REBOOT)
    {
        UART_PRINT("[OTA] Rejected; rebooting to roll back\n\r");
        psa_fwu_request_reboot();
    }
    UART_PRINT("[OTA] Reject returned %d\n\r", ret);
    return ret;
}

int ota_glue_update(const char *serverUrl, ota_status_cb status, void *ctx)
{
    static OtaUpdateEntry_t entries[OTA_MAX_UPDATES];
    char msg[96];
    int count, i, sel = -1, ret;

    if (s_trial)
    {
        status(ctx, "ota: refused, trial image running (accept or reject first)");
        return -1;
    }

    status(ctx, "ota: querying server");
    count = OTA_SERVER_queryUpdates(serverUrl, entries, OTA_MAX_UPDATES);
    if (count <= 0)
    {
        status(ctx, "ota: query failed or no images");
        return -1;
    }
    for (i = 0; i < count; i++)
    {
        if (entries[i].slot1Id == Vendor_Image_Slot_1 && entries[i].slot2Id == Vendor_Image_Slot_2)
        {
            sel = i;
            break;
        }
    }
    if (sel < 0)
    {
        status(ctx, "ota: server offers no vendor image");
        return -1;
    }
    snprintf(msg, sizeof(msg), "ota: downloading %.40s version %.16s", entries[sel].filename, entries[sel].version);
    status(ctx, msg);

    /* from TI: menu 2a choices [2] download and [3] install, then 2b [1] reboot */
    if (OTA_FWU_checkDownloadOrder(entries[sel].slot1Id, entries[sel].slot2Id) != 0)
    {
        status(ctx, "ota: download order check failed");
        return -1;
    }
    ret = OTA_SERVER_downloadUpdate(serverUrl, &entries[sel]);
    if (ret != 0)
    {
        status(ctx, "ota: download failed");
        OTA_FWU_cancelAndCleanCandidates();
        return -1;
    }
    if (OTA_FWU_checkBundleForInstall() != 0)
    {
        status(ctx, "ota: bundle check failed");
        OTA_FWU_cancelAndCleanCandidates();
        return -1;
    }
    ret = psa_fwu_install();
    if (ret != PSA_SUCCESS_REBOOT)
    {
        snprintf(msg, sizeof(msg), "ota: install failed %d", ret);
        status(ctx, msg);
        OTA_FWU_cancelAndCleanCandidates();
        return -1;
    }
    status(ctx, "ota: staged, rebooting into trial");
    vTaskDelay(pdMS_TO_TICKS(500));   /* let the status message leave */
    ret = psa_fwu_request_reboot();
    snprintf(msg, sizeof(msg), "ota: reboot request failed %d", ret);
    status(ctx, msg);
    return -1;
}
