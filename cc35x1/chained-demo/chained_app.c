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
 * Modified by Fox Red Labs, 2026. Derived from TI's demos/mqtt_client/mqtt_client_app.c (SimpleLink Wi-Fi SDK 10.20.00.39).
 * The TI copyright and license above are kept as in the original file.
 */
/*
 * chained_app.c - one LP-EM-CC35X1 image that chains TI's separate demos:
 *
 *   1. first boot: Wi-Fi credentials from a phone over Bluetooth LE
 *      (TI ble_wifi_provisioning: SimpleLink Connect app, GATT service 0xCC00);
 *   2. credentials saved to NV storage, so a reboot rejoins without the phone;
 *   3. Wi-Fi join and MQTT client (TI mqtt_client) publishing telemetry and
 *      taking commands on one topic;
 *   4. over-the-air update started by an MQTT message (TI ota_example), with
 *      TI's trial step: the new image must be accepted or it is rolled back.
 *
 * Replaces TI's mqtt_client_app.c (whose structure it follows: message queue,
 * OnWifiEvent, OnMqttEvent, button handlers). Built on SimpleLink Wi-Fi SDK
 * 10.20.00.39. Lines marked "from TI" come from TI's mqtt_client_app.c
 * (BSD-3-Clause, Copyright (c) 2024 Texas Instruments Incorporated).
 * Nothing here has run on a board yet. Fox Red Labs, 2026-10-08.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "ti_drivers_config.h"
#include <ti/drivers/Power.h>
#include "osi_kernel.h"
#include "uart_term.h"

#include "wifi_if.h"
#include "wifi_if_creds.h"
#include "tcpip_if.h"
#include "mqtt_if.h"
#include "led_if.h"
#include "button_if.h"

#include "cred_record.h"
#include "cmd_parse.h"
#include "cred_nv.h"
#include "prov_glue.h"
#include "ota_glue.h"

#include "chained_config.h"

#ifndef CHAINED_FW_VERSION
#define CHAINED_FW_VERSION "0.0.1.0"   /* build.sh passes the image version */
#endif

#define JOIN_TIMEOUT_MS   15000

enum
{
    EV_MQTT_CONNECTED,
    EV_MQTT_DISCONNECTED,
    EV_CMD,
    EV_PUBLISH_NOW,
    EV_FORGET_WIFI,
};

struct msgQueue
{
    int      event;
    char    *payload;   /* EV_CMD: malloc'd copy, freed by the main loop */
    uint16_t len;
};

static OsiMsgQ_t appQueue;
static ip4addr_t gIp4Addr, gIp4Mask, gIp4GW;
static int connected;

/* MQTT_IF keeps these pointers: they must outlive the client. */
static char tTelemetry[64], tStatus[64], tCmd[64];
static char clientId[32];
static MQTTClient_Will willParams;
static MQTT_IF_ClientParams_t clientParams;
static MQTTClient_ConnParams connParams;

/* from TI */
static char *IP4ToStr(ip4addr_t ipAddress)
{
    static char ip4str[16];
    uint8_t *pU8 = (uint8_t *)&ipAddress;
    sprintf(ip4str, "%d.%d.%d.%d", pU8[0], pU8[1], pU8[2], pU8[3]);
    return ip4str;
}

static void post(int event, char *payload, uint16_t len)
{
    struct msgQueue q = { event, payload, len };
    if (osi_MsgQWrite(&appQueue, &q, 100, OSI_FLAG_NOT_FROM_INTR) < 0)
    {
        UART_PRINT("[APP] queue full, event %d dropped\r\n", event);
        free(payload);
    }
}

/* from TI mqtt_client_app.c */
static void OnWifiEvent(WifiConnStatus_e status, void *params)
{
    if (status == WIFI_STATUS_CONNECTED_IP)
    {
        TCPIP_IF_getIp4Addr(params, &gIp4Addr, &gIp4Mask, &gIp4GW);
        UART_PRINT("[WIFI] connected, addr=%s\r\n", IP4ToStr(gIp4Addr));
    }
    else
    {
        UART_PRINT("[WIFI] event %d\r\n", status);
    }
}

static void ButtonSw1Handler(BUTTON_IF_events_bm events)
{
    if (events & BUTTON_IF_EV_CLICKED)
    {
        post(EV_PUBLISH_NOW, NULL, 0);
    }
}

static void ButtonSw2Handler(BUTTON_IF_events_bm events)
{
    if (events & BUTTON_IF_EV_LONG_CLICKED)
    {
        post(EV_FORGET_WIFI, NULL, 0);
    }
}

static void CmdCB(char *topic, uint8_t *payload, uint16_t len, uint8_t qos)
{
    /* TI: topic and payload are freed after the callback returns; copy. */
    char *p = malloc(len + 1u);
    (void)topic;
    (void)qos;
    if (p)
    {
        memcpy(p, payload, len);
        p[len] = 0;
        post(EV_CMD, p, len);
    }
}

static void OnMqttEvent(MQTTClient_Handle h, int32_t event, void *args)
{
    (void)h;
    (void)args;
    switch (event)
    {
        case MQTT_EVENT_CONNACK:
            post(EV_MQTT_CONNECTED, NULL, 0);
            break;
        case MQTT_EVENT_SERVER_DISCONNECT:
            post(EV_MQTT_DISCONNECTED, NULL, 0);
            break;
        default:
            UART_PRINT("[MQTT] event %d\r\n", (int)event);
            break;
    }
}

static int read_chip_temp(int *tempC)
{
#if CHAINED_READ_CHIP_TEMP
    CalibratorGetTemperature_t t = {0};
    CalibratorCommandWrapper_t w;

    w.calibratorAction = CALIBRATOR_ACTION_GET_TEMPERATURE;
    w.calibratorCommandParams = &t;
    if (Wlan_Get(WLAN_GET_CALIBRATOR_TEMPERATURE, &w) >= 0)
    {
        *tempC = t.measuredTemp;
        return 1;
    }
#else
    (void)tempC;
#endif
    return 0;
}

static void publish_str(MQTTClient_Handle h, char *topic, const char *s)
{
    if (connected)
    {
        MQTT_IF_publish(h, topic, (char *)s, (unsigned short)strlen(s), MQTT_QOS_0);
    }
    UART_PRINT("[PUB] %s %s\r\n", topic, s);
}

static void publish_telemetry(MQTTClient_Handle h)
{
    static uint32_t count;
    char json[128];
    int temp = 0;
    int have = read_chip_temp(&temp);

    count++;
    if (telemetry_json(json, sizeof(json), count, osi_GetTimeMS() / 1000u, have, temp, CHAINED_FW_VERSION) > 0)
    {
        publish_str(h, tTelemetry, json);
    }
}

static void ota_status(void *ctx, const char *msg)
{
    publish_str((MQTTClient_Handle)ctx, tStatus, msg);
}

static void forget_and_reboot(MQTTClient_Handle h)
{
    int rc = cred_nv_erase();
    char msg[48];
    snprintf(msg, sizeof(msg), "forget-wifi: erase %s, rebooting", rc == 0 ? "ok" : "failed");
    publish_str(h, tStatus, msg);
    osi_Sleep(1);
    Power_reset();
}

/* Bluetooth provisioning until a join succeeds; saves the credentials. */
static void provision_over_ble(void *hWifiConn)
{
    char ssid[WLAN_SSID_MAX_LENGTH + 1], pass[CRED_PASS_MAX + 1];
    uint8_t wlanSec, passLen;
    cred_t c;
    int rc;

    UART_PRINT("[PROV] starting Bluetooth LE provisioning\r\n");
    if (prov_init() != 0 || prov_ble_start() != 0)
    {
        UART_PRINT("[PROV] Bluetooth start failed; stopping here\r\n");
        while (1)
        {
            osi_Sleep(10);
        }
    }
    advConfigure();
    advEnable();
    LED_IF_set(CONFIG_LED_BLUE, 100);
    UART_PRINT("[PROV] advertising; open SimpleLink Connect, pick this board, "
               "service \"Wi-Fi Provisioning Over BLE\", enter the network and press Connect\r\n");

    while (1)
    {
        prov_wait_credentials(ssid, &wlanSec, pass, &passLen);
        rc = cred_from_parts(&c, (const uint8_t *)ssid, strlen(ssid), (const uint8_t *)pass, passLen,
                             cred_sec_from_wlan(wlanSec));
        memset(pass, 0, sizeof(pass));
        if (rc != CRED_OK)
        {
            UART_PRINT("[PROV] credentials refused (%d); send them again\r\n", rc);
            prov_notify(0);
            continue;
        }
        WIFI_IF_setCredentials(c.ssid, (uint8_t)cred_sec_to_wlan(c.sec), c.pass, c.pass_len);
        UART_PRINT("[PROV] joining %s\r\n", c.ssid);
        rc = WIFI_IF_connectCredentials(hWifiConn, WIFI_SERVICE_LVL_IP, JOIN_TIMEOUT_MS);
        prov_notify(rc == OSI_OK);
        if (rc == OSI_OK)
        {
            rc = cred_nv_save(&c);
            UART_PRINT("[PROV] joined; credentials %s\r\n", rc == 0 ? "saved" : "NOT saved");
            break;
        }
        UART_PRINT("[PROV] join failed (%d); send the credentials again\r\n", rc);
    }
    cred_wipe(&c);
    prov_adv_stop();
    LED_IF_set(CONFIG_LED_BLUE, 0);
}

void *mainThread(void *args)
{
    void *hWifiConn = NULL;
    MQTT_IF_InitParams_t initParams = {0};
    MQTTClient_Handle hMqtt;
    WlanMacAddress_t mac;
    cred_t c;
    int rc, haveCred, trial, everConnected = 0;
    uint32_t trialDeadline, nextPublish;

    (void)args;
    Board_init();
    InitTerm();
    LED_IF_init();
    BUTTON_IF_init();
    BUTTON_IF_registertCallback(CONFIG_BUTTON_SW1, CONFIG_GPIO_BUTTON_SW1_INPUT, ButtonSw1Handler,
                                BUTTON_IF_EV_CLICKED, 1000U);
    BUTTON_IF_registertCallback(CONFIG_BUTTON_SW2, CONFIG_GPIO_BUTTON_SW2_INPUT, ButtonSw2Handler,
                                BUTTON_IF_EV_CLICKED | BUTTON_IF_EV_LONG_CLICKED, 3000U);

    UART_PRINT("\r\n==== Fox Red Labs chained demo %s: BLE provisioning, Wi-Fi, MQTT, OTA ====\r\n",
               CHAINED_FW_VERSION);

    if (strlen(CHAINED_CMD_TOKEN) < CMD_TOKEN_MIN)
    {
        UART_PRINT("[APP] CHAINED_CMD_TOKEN is a placeholder or too short: every MQTT command will be refused\r\n");
    }
    if (strlen(CHAINED_OTA_HOST) == 0)
    {
        UART_PRINT("[APP] CHAINED_OTA_HOST is empty: every update command will be refused\r\n");
    }

    rc = osi_MsgQCreate(&appQueue, "appQueue", sizeof(struct msgQueue), 10);
    ASSERT_GENERAL(rc == OSI_OK);

    /* OTA state first: a trial image must be judged on this boot */
    trial = ota_glue_boot();
    trialDeadline = osi_GetTimeMS() + CHAINED_TRIAL_TIMEOUT_S * 1000u;
    ota_glue_select_wifi_fw_slot();

    rc = WIFI_IF_init(true);
    ASSERT_GENERAL(rc == OSI_OK);
    rc = MQTT_IF_init(initParams);
    ASSERT_GENERAL(rc == OSI_OK);

    /* Stored credentials, else the optional compiled-in network */
    haveCred = (cred_nv_load(&c) == CRED_OK);
    UART_PRINT("[APP] stored credentials: %s\r\n", haveCred ? c.ssid : "none");
    if (!haveCred && strlen(CHAINED_DEFAULT_SSID) > 0)
    {
        haveCred = (cred_from_parts(&c, (const uint8_t *)CHAINED_DEFAULT_SSID, strlen(CHAINED_DEFAULT_SSID),
                                    (const uint8_t *)CHAINED_DEFAULT_PASS, strlen(CHAINED_DEFAULT_PASS),
                                    CHAINED_DEFAULT_SEC) == CRED_OK);
    }
    if (haveCred)
    {
        WIFI_IF_setCredentials(c.ssid, (uint8_t)cred_sec_to_wlan(c.sec), c.pass, c.pass_len);
    }
    cred_wipe(&c);

    rc = WIFI_IF_start(OnWifiEvent, WIFI_SERVICE_LVL_IP, JOIN_TIMEOUT_MS, &hWifiConn);
    while (rc != OSI_OK && haveCred && trial)
    {
        /* A trial image that cannot rejoin is rolled back, not re-provisioned */
        if ((int32_t)(osi_GetTimeMS() - trialDeadline) > 0)
        {
            UART_PRINT("[OTA] trial image could not rejoin Wi-Fi; rejecting\r\n");
            ota_glue_reject();
        }
        rc = WIFI_IF_connectCredentials(hWifiConn, WIFI_SERVICE_LVL_IP, JOIN_TIMEOUT_MS);
    }
    if (rc != OSI_OK)
    {
        provision_over_ble(hWifiConn);
    }
    while (gIp4Addr == 0)   /* from TI */
    {
        osi_uSleep(1000);
    }

    ota_glue_set_clock();

    /* MQTT client with run-time settings (TI's mqtt_settings.h stays as shipped) */
    mac.roleType = WLAN_ROLE_STA;
    Wlan_Get(WLAN_GET_MACADDRESS, &mac);
    snprintf(clientId, sizeof(clientId), "frl-cc35x1-%02x%02x%02x",
             mac.pMacAddress[3], mac.pMacAddress[4], mac.pMacAddress[5]);
    topic_join(tTelemetry, sizeof(tTelemetry), CHAINED_TOPIC_PREFIX, "telemetry");
    topic_join(tStatus, sizeof(tStatus), CHAINED_TOPIC_PREFIX, "status");
    topic_join(tCmd, sizeof(tCmd), CHAINED_TOPIC_PREFIX, "cmd");
    willParams.willTopic = tStatus;
    willParams.willMsg = "offline";
    willParams.willQos = MQTT_QOS_0;
    willParams.retain = false;
    clientParams.clientID = clientId;
    clientParams.keepaliveTime = 0;   /* TI's default */
    clientParams.cleanConnect = true;
    clientParams.willParams = &willParams;
    connParams.netconnFlags = MQTTCLIENT_NETCONN_URL;
    connParams.serverAddr = CHAINED_BROKER_HOST;
    connParams.port = CHAINED_BROKER_PORT;

    UART_PRINT("[MQTT] client %s, broker %s:%d, commands on %s\r\n", clientId, CHAINED_BROKER_HOST,
               CHAINED_BROKER_PORT, tCmd);
    hMqtt = MQTT_IF_clientCreate(&clientParams, OnMqttEvent);
    ASSERT_GENERAL((uint32_t)hMqtt);
    rc = MQTT_IF_clientConnect(hMqtt, &connParams);
    if (rc != 0)
    {
        UART_PRINT("[MQTT] connect failed %d (DNS?)\r\n", rc);
    }

    nextPublish = osi_GetTimeMS() + CHAINED_PUBLISH_PERIOD_S * 1000u;
    while (1)
    {
        struct msgQueue q;
        int32_t wait = (int32_t)(nextPublish - osi_GetTimeMS());

        /* Checked on every pass: reconnect events can arrive every few seconds */
        if (trial && !everConnected && (int32_t)(osi_GetTimeMS() - trialDeadline) > 0)
        {
            UART_PRINT("[OTA] trial image never reached the broker; rejecting\r\n");
            ota_glue_reject();
        }
        if (wait <= 0)
        {
            if (connected)
            {
                publish_telemetry(hMqtt);
            }
            nextPublish = osi_GetTimeMS() + CHAINED_PUBLISH_PERIOD_S * 1000u;
            continue;
        }
        if (osi_MsgQRead(&appQueue, &q, (OsiTime_t)wait) != OSI_OK)
        {
            continue;   /* period elapsed */
        }

        switch (q.event)
        {
            case EV_MQTT_CONNECTED:
                connected = 1;
                if (!everConnected)
                {
                    /* MQTT_IF re-subscribes on reconnect by itself */
                    MQTT_IF_subscribe(hMqtt, tCmd, MQTT_QOS_0, CmdCB);
                }
                everConnected = 1;
                publish_str(hMqtt, tStatus, trial ? "online, trial image " CHAINED_FW_VERSION
                                                    ": send 'ota accept' or 'ota reject'"
                                                  : "online " CHAINED_FW_VERSION);
                publish_telemetry(hMqtt);
                break;

            case EV_MQTT_DISCONNECTED:
                connected = 0;
                UART_PRINT("[MQTT] broker closed the connection; reconnecting\r\n");
                osi_Sleep(2);
                MQTT_IF_clientConnect(hMqtt, &connParams);
                break;

            case EV_PUBLISH_NOW:
                publish_telemetry(hMqtt);
                break;

            case EV_FORGET_WIFI:
                forget_and_reboot(hMqtt);
                break;

            case EV_CMD:
            {
                cmd_t cmd;
                cmd_parse((const uint8_t *)q.payload, q.len, CHAINED_CMD_TOKEN, CHAINED_OTA_HOST, &cmd);
                memset(q.payload, 0, q.len);   /* holds the token */
                UART_PRINT("[CMD] %s\r\n", cmd_name(cmd.type));
                switch (cmd.type)
                {
                    case CMD_PUBLISH:
                        publish_telemetry(hMqtt);
                        break;
                    case CMD_LED:
                        LED_IF_toggle(cmd.led == LED_RED ? CONFIG_LED_RED
                                      : cmd.led == LED_GREEN ? CONFIG_LED_GREEN : CONFIG_LED_BLUE, 100);
                        break;
                    case CMD_OTA_START:
                        ota_glue_update(cmd.url, ota_status, hMqtt);
                        break;
                    case CMD_OTA_ACCEPT:
                        publish_str(hMqtt, tStatus, trial ? "ota: accepting trial image" : "ota: no trial image");
                        if (trial)
                        {
                            osi_Sleep(1);
                            ota_glue_accept();
                        }
                        break;
                    case CMD_OTA_REJECT:
                        publish_str(hMqtt, tStatus, trial ? "ota: rejecting trial image" : "ota: no trial image");
                        if (trial)
                        {
                            osi_Sleep(1);
                            ota_glue_reject();
                        }
                        break;
                    case CMD_FORGET_WIFI:
                        forget_and_reboot(hMqtt);
                        break;
                    case CMD_UNAUTHORIZED:
                        /* no reply on the broker: do not help a guesser */
                        break;
                    default:
                        publish_str(hMqtt, tStatus, "unknown command");
                        break;
                }
                free(q.payload);
                break;
            }

            default:
                break;
        }
    }
}
