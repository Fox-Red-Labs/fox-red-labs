
/* ---- Fox Red Labs: entry points for the ESPHome Wi-Fi backend. ti35_build.py appends this file to the build copy of
 * TI's wifi_if.c (mqtt_client example, Wi-Fi SDK 10.20.00.39), so it sees m_wifiCtx. TI's file is otherwise unchanged. ---- */
int FRL_wifi_init(void) { return WIFI_IF_init(true); }

int FRL_wifi_sta_up(void)
{
    static int up = 0;
    if (up) return 0;
    RoleUpStaCmd_t p;
    memset(&p, 0, sizeof(p));
    strncpy((char *)p.countryDomain, "00", 3);  /* world-wide domain, as TI's WIFI_IF_start does */
    TCPIP_IF_setInterfaceState(m_wifiCtx.hStaNetif, E_TCPIP_IF_UP);
    int rc = Wlan_RoleUp(WLAN_ROLE_STA, &p, WLAN_WAIT_FOREVER);
    up = (rc >= 0);
    return rc;
}

int FRL_wifi_connect(const char *ssid, int ssid_len, const char *pass, int pass_len)
{
    return Wlan_Connect((const signed char *)ssid, ssid_len, NULL,
                        pass_len ? WLAN_SEC_TYPE_WPA_WPA2 : WLAN_SEC_TYPE_OPEN, pass, (char)pass_len, 0);
}

int FRL_wifi_disconnect(void) { return Wlan_Disconnect(WLAN_ROLE_STA, NULL); }
int FRL_wifi_link(void) { return (m_wifiCtx.Status & (1 << STATUS_BIT_STA_CONNECTION)) ? 1 : 0; }
void *FRL_wifi_netif(void) { return m_wifiCtx.hStaNetif; }
const unsigned char *FRL_wifi_bssid(void) { return (const unsigned char *)m_wifiCtx.currentConn.BSSID; }
const char *FRL_wifi_ssid(void) { return (const char *)m_wifiCtx.currentConn.SSID; }

/* Mirror of BeaconRssi_t (ti/net/wifi_stack/inc_shared_w_fw/inc_common_fw/cc3xxx_public_commands.h), filled by Wlan_Get(WLAN_GET_RSSI). */
typedef struct __attribute__((packed)) { uint8_t role_id; uint8_t pad[3]; uint32_t missed_beacons; uint8_t snr_data; uint8_t snr_beacon; int8_t rssi_data; int8_t rssi_beacon; } FrlBeaconRssi_t;
int FRL_wifi_rssi(void)
{
    FrlBeaconRssi_t r;
    memset(&r, 0, sizeof(r));
    r.role_id = (uint8_t)WLAN_ROLE_STA;
    if (Wlan_Get(WLAN_GET_RSSI, &r) < 0) return 127;
    return r.rssi_beacon;
}

int FRL_mac_address(unsigned char out[6])
{
    WlanMacAddress_t m;
    memset(&m, 0, sizeof(m));
    m.roleType = WLAN_ROLE_STA;
    if (!m_wifiCtx.bIsStarted || Wlan_Get(WLAN_GET_MACADDRESS, &m) < 0) return -1;
    memcpy(out, m.pMacAddress, 6);
    return 0;
}

extern int g_frl_channel;  /* set in the connect event (see ti35_build.py) */
int FRL_wifi_channel(void) { return FRL_wifi_link() ? g_frl_channel : 0; }

/* Console print for ESPHome's C++ side. TI's osi_kernel.h declares Report() without C linkage guards, so C++ files that
 * see it get a mangled name; they call this C wrapper instead. */
#include <stdarg.h>
#include <stdio.h>
void FRL_log(const char *fmt, ...)
{
    char buf[320];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Report("%s", buf);
}

/* lwIP mDNS for ESPHome's mdns component (shim/mdns_cc35xx.cpp). In C because TI's lwIP port declares its core lock
 * without C linkage guards. Up to 2 services with 16 TXT items each; the strings are copied. */
#include "lwip/apps/mdns.h"
#include "lwip/tcpip.h"
#define FRL_MDNS_SVC 2
#define FRL_MDNS_TXT 16
static char frl_txt[FRL_MDNS_SVC][FRL_MDNS_TXT][64];
static int frl_ntxt[FRL_MDNS_SVC];
static int frl_mdns_netif_added = 0;

static void frl_txt_cb(struct mdns_service *service, void *userdata)
{
    int s = (int)(intptr_t)userdata;
    for (int i = 0; i < frl_ntxt[s]; i++)
        mdns_resp_add_service_txtitem(service, frl_txt[s][i], (u8_t)strlen(frl_txt[s][i]));
}

int FRL_mdns_add(int slot, const char *host, const char *service, int udp, int port, const char *const *txt, int ntxt)
{
    struct netif *n = (struct netif *)m_wifiCtx.hStaNetif;
    static int inited = 0;
    int rc;
    if (n == NULL || slot < 0 || slot >= FRL_MDNS_SVC) return -1;
    frl_ntxt[slot] = ntxt > FRL_MDNS_TXT ? FRL_MDNS_TXT : ntxt;
    for (int i = 0; i < frl_ntxt[slot]; i++) {
        strncpy(frl_txt[slot][i], txt[i], sizeof(frl_txt[slot][i]) - 1);
        frl_txt[slot][i][sizeof(frl_txt[slot][i]) - 1] = 0;
    }
    LOCK_TCPIP_CORE();
    if (!inited) { mdns_resp_init(); inited = 1; }
    if (!frl_mdns_netif_added) frl_mdns_netif_added = (mdns_resp_add_netif(n, host, 120) == ERR_OK);
    rc = mdns_resp_add_service(n, host, service, udp ? DNSSD_PROTO_UDP : DNSSD_PROTO_TCP, (u16_t)port, 120, frl_txt_cb, (void *)(intptr_t)slot);
    UNLOCK_TCPIP_CORE();
    return rc;
}

void FRL_mdns_announce(void)
{
    struct netif *n = (struct netif *)m_wifiCtx.hStaNetif;
    if (n == NULL || !frl_mdns_netif_added) return;
    LOCK_TCPIP_CORE();
    mdns_resp_announce(n);
    UNLOCK_TCPIP_CORE();
}

void FRL_mdns_remove(void)
{
    struct netif *n = (struct netif *)m_wifiCtx.hStaNetif;
    if (n == NULL || !frl_mdns_netif_added) return;
    LOCK_TCPIP_CORE();
    mdns_resp_remove_netif(n);
    UNLOCK_TCPIP_CORE();
    frl_mdns_netif_added = 0;
}

/* Scanning: TI's WIFI_IF_scan() starts it; the scan-result event fills m_wifiCtx.netEntries and (one line added to the
 * event handler by ti35_build.py) sets g_frl_scan_done. */
extern volatile int g_frl_scan_done;
int FRL_wifi_scan_start(void)
{
    g_frl_scan_done = 0;
    m_wifiCtx.netEntriesSize = 0;
    return WIFI_IF_scan(WLAN_ROLE_STA);
}
int FRL_wifi_scan_done(void) { return g_frl_scan_done; }
int FRL_wifi_scan_count(void) { return (int)m_wifiCtx.netEntriesSize; }
int FRL_wifi_scan_entry(int i, char ssid[33], unsigned char bssid[6], int *rssi, int *channel, int *secured)
{
    if (i < 0 || i >= (int)m_wifiCtx.netEntriesSize) return -1;
    WlanNetworkEntry_t *e = &m_wifiCtx.netEntries[i];
    int n = e->SsidLen > 32 ? 32 : e->SsidLen;
    memcpy(ssid, e->Ssid, n);
    ssid[n] = 0;
    memcpy(bssid, e->Bssid, 6);
    *rssi = e->Rssi;
    *channel = e->Channel;
    *secured = e->SecurityInfo != 0;
    return 0;
}
