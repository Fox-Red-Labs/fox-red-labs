/* Shared start-up for the DNS sinkhole firmware (written fresh for this repo).
 * It builds inside a copy of TI's SimpleLink Wi-Fi SDK demos/mqtt_client folder, which supplies wifi_if, tcpip_if, the
 * syscfg and the makefile (BSD-3 licence, TI's files stay in the copy and are not stored here). */
#ifndef DNS_BOOT_H
#define DNS_BOOT_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "ti_drivers_config.h"
#include "osi_kernel.h"
#include "uart_term.h"
#include "wifi_if.h"
#include "tcpip_if.h"
#include "led_if.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/tcpip.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"

static volatile ip4addr_t gIp4Addr, gIp4Mask, gIp4GW;

static char *ip4str(ip4addr_t a)
{
    static char s[16];
    uint8_t *b = (uint8_t *)&a;
    sprintf(s, "%d.%d.%d.%d", b[0], b[1], b[2], b[3]);
    return s;
}

static void on_wifi(WifiConnStatus_e status, void *params)
{
    if (status == WIFI_STATUS_CONNECTED_IP) {
        ip4addr_t a = 0, m = 0, g = 0;
        TCPIP_IF_getIp4Addr(params, &a, &m, &g);
        if (a) {
            gIp4Mask = m;
            gIp4GW = g;
            gIp4Addr = a;
            UART_PRINT("IP %s gateway %s\r\n", ip4str(a), ip4str(g));
        }
    } else {
        UART_PRINT("wifi event %d\r\n", (int)status);
    }
}

/* Board, console, Wi-Fi join with the credentials in wifi_settings.h. Returns 0 when an IPv4 address is held. */
static int dns_boot(const char *name)
{
    void *hWifi;
    Board_init();
    InitTerm();
    LED_IF_init();
    UART_PRINT("\r\n*** %s ***\r\n", name);
    if (WIFI_IF_init(true) != OSI_OK) { UART_PRINT("wifi init failed\r\n"); return -1; }
    osi_Sleep(1);
    if (WIFI_IF_start(on_wifi, WIFI_SERVICE_LVL_IP, 20000, &hWifi) != OSI_OK) { UART_PRINT("wifi join failed\r\n"); return -2; }
    while (gIp4Addr == 0) osi_uSleep(1000);
    return 0;
}
#endif
