/*
 * cred_nv.c - see cred_nv.h. Fox Red Labs, 2026-10-08.
 *
 * Uses the same NVOCMP instance TI's osi_filesystem.c uses for Bluetooth bonds
 * and Wi-Fi profiles (NVOCMP's init is once per reset, so a second caller is
 * safe). Item: system ID NVINTF_SYSID_APP, item 1, sub 0. With SysConfig's SPS
 * module on (it is, in TI's mqtt_client.syscfg), NVOCMP encrypts item payloads
 * with a key derived from the chip's hardware unique key.
 */
#include <string.h>

#include "nvintf.h"
#include "nvocmp.h"
#include "wlan_if.h"

#include "cred_nv.h"

#define CRED_NV_ITEM 1

static NVINTF_nvFuncts_t s_nv;
static int s_ready;

static int nv_ready(void)
{
    if (!s_ready)
    {
        NVOCMP_loadApiPtrs(&s_nv);
        if (s_nv.initNV(NULL) != NVINTF_SUCCESS)
        {
            return -1;
        }
        s_ready = 1;
    }
    return 0;
}

static NVINTF_itemID_t cred_id(void)
{
    NVINTF_itemID_t id;
    id.systemID = NVINTF_SYSID_APP;
    id.itemID = CRED_NV_ITEM;
    id.subID = 0;
    return id;
}

int cred_nv_load(cred_t *c)
{
    uint8_t rec[CRED_RECORD_SIZE];
    uint8_t st;
    int rc;

    memset(c, 0, sizeof(*c));
    if (nv_ready() != 0)
    {
        return -100;
    }
    st = s_nv.readItem(cred_id(), 0, sizeof(rec), rec);
    if (st != NVINTF_SUCCESS)
    {
        return -100 - (int)st;   /* NVINTF_NOTFOUND when never provisioned */
    }
    rc = cred_decode(rec, sizeof(rec), c);
    memset(rec, 0, sizeof(rec));
    return rc;
}

int cred_nv_save(const cred_t *c)
{
    uint8_t rec[CRED_RECORD_SIZE];
    uint8_t st;

    if (nv_ready() != 0)
    {
        return -100;
    }
    if (cred_encode(c, rec, sizeof(rec)) != CRED_RECORD_SIZE)
    {
        return CRED_E_ARG;
    }
    st = s_nv.writeItem(cred_id(), sizeof(rec), rec);
    memset(rec, 0, sizeof(rec));
    return (st == NVINTF_SUCCESS) ? 0 : -100 - (int)st;
}

int cred_nv_erase(void)
{
    uint8_t st;

    if (nv_ready() != 0)
    {
        return -100;
    }
    st = s_nv.deleteItem(cred_id());
    return (st == NVINTF_SUCCESS || st == NVINTF_NOTFOUND) ? 0 : -100 - (int)st;
}

int cred_sec_to_wlan(uint8_t sec)
{
    switch (sec)
    {
        case CRED_SEC_OPEN: return WLAN_SEC_TYPE_OPEN;
        case CRED_SEC_WPA2: return WLAN_SEC_TYPE_WPA_WPA2;
        case CRED_SEC_WPA3: return WLAN_SEC_TYPE_WPA3;
        default:            return -1;
    }
}

int cred_sec_from_wlan(int wlanSec)
{
    switch (wlanSec)
    {
        case WLAN_SEC_TYPE_OPEN:     return CRED_SEC_OPEN;
        case WLAN_SEC_TYPE_WPA_WPA2: return CRED_SEC_WPA2;
        case WLAN_SEC_TYPE_WPA3:     return CRED_SEC_WPA3;
        default:                     return -1;
    }
}
