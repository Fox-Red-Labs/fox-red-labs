#!/bin/bash
# Build TI's mqtt_client with Home Assistant MQTT discovery for the LP-EM-CC35X1, in a copy outside the repo.
#   HOME_SSID="<network name>" WIFI_ENV=<file with WIFI_PASSWORD=...> BROKER_HOST=<ip> BROKER_PORT=<port> ./build.sh
# The network name and password go into the firmware copy only (never into the repo); program the Network Terminal over it afterwards.
# Changes to TI's example: mqtt_if.c passes the retain flag (0x100) and QoS 0 to lwIP; the main loop wakes every 10 s and
# publishes the readings; on connect it publishes four retained discovery configs (temperature and accelerometer x, y, z).
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; RUN="$PWD/out/cc35x1/mqttha"
SDK=/Applications/ti/simplelink_wifi_sdk_10_20_00_39; TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh; CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
rm -rf "$RUN"; mkdir -p "$RUN"; C="$RUN/m"; cp -R "$SDK/examples/rtos/LP_EM_CC35X1/demos/mqtt_client" "$C"
python3 - "$C" "$HOME_SSID" "$WIFI_ENV" "$BROKER_HOST" "$BROKER_PORT" <<'PY'
import sys,re
C,ssid,envf,host,port=sys.argv[1:6]
pw=[l.split('=',1)[1].strip() for l in open(envf) if l.startswith('WIFI_PASSWORD=')][0].strip('"\'')
def edit(p,f):
    t=open(C+'/'+p).read(); t=f(t); open(C+'/'+p,'w').write(t)
edit('wifi_settings.h',lambda t:t.replace('"Example_SSID"','"%s"'%ssid).replace('"Example_PASS"','"%s"'%pw))
edit('mqtt_settings.h',lambda t:t.replace('"broker.hivemq.com"','"%s"'%host).replace('MQTT_CONNECTION_PORT_NUMBER     1883','MQTT_CONNECTION_PORT_NUMBER     %s'%port))
old='mqtt_publish(pClient->hLWIPClient, topic, payload, payloadLen, 0, 0, LwipCB_mqttPubAck, pClient);'
edit('mqtt_if.c',lambda t:(t if old in t else (_ for _ in ()).throw(SystemExit('mqtt_if patch point missing'))).replace(old,'mqtt_publish(pClient->hLWIPClient, topic, payload, payloadLen, 0, (flags & 0x100) ? 1 : 0, LwipCB_mqttPubAck, pClient);'))
def app(t):
    # accelerometer values kept per axis
    parts=t.split('accXYZ = (rxBuffer[0]);'); assert len(parts)==4,len(parts)
    t=parts[0]+'accXYZ = (rxBuffer[0]); accVals[0] = accXYZ;'+parts[1]+'accXYZ = (rxBuffer[0]); accVals[1] = accXYZ;'+parts[2]+'accXYZ = (rxBuffer[0]); accVals[2] = accXYZ;'+parts[3]
    t=t.replace('enum{\n    APP_MQTT_PUBLISH,','static int accVals[3];\nenum{\n    APP_MQTT_PUBLISH,',1)
    # periodic wake-up
    a='osi_MsgQRead(&appQueue, (char*)&queueElement, OSI_WAIT_FOREVER);'; assert a in t
    t=t.replace(a,'if (osi_MsgQRead(&appQueue, (char*)&queueElement, 10000) != OSI_OK) { if (!connected) continue; queueElement.event = APP_MQTT_PUBLISH_SENSORS; }')
    # state topics after the existing sensors publish
    b='''                MQTT_IF_publish(hMqttConn,
                                "cc35xx/Sensors",
                                sensorsStr,
                                strlen(sensorsStr),
                                MQTT_QOS_2);'''; assert b in t
    t=t.replace(b,b+'''
                {
                    char sv[16]; const char *tp[4] = {"cc35x1/t", "cc35x1/ax", "cc35x1/ay", "cc35x1/az"};
                    int vv[4] = {temperature, accVals[0], accVals[1], accVals[2]};
                    for (int k = 0; k < 4; k++) { sprintf(sv, "%d", vv[k]); MQTT_IF_publish(hMqttConn, (char*)tp[k], sv, strlen(sv), MQTT_QOS_0); osi_uSleep(100000); }
                }''')
    # discovery after the subscriptions
    c='ASSERT_GENERAL(ret == 0);\n'; i=t.index('APP_MQTT_CONNECTED\\r\\n'); j=t.index(c,i)+len(c)
    disc='''                {
                    static const char *cfg[4] = {
                      "homeassistant/sensor/cc35x1_temp/config", "homeassistant/sensor/cc35x1_ax/config",
                      "homeassistant/sensor/cc35x1_ay/config", "homeassistant/sensor/cc35x1_az/config" };
                    static const char *pl[4] = {
                      "{\\"name\\":\\"Temperature\\",\\"uniq_id\\":\\"cc35x1_temp\\",\\"stat_t\\":\\"cc35x1/t\\",\\"unit_of_meas\\":\\"\\xC2\\xB0""C\\",\\"dev_cla\\":\\"temperature\\",\\"dev\\":{\\"ids\\":[\\"cc35x1\\"],\\"name\\":\\"LP-EM-CC35X1\\"}}",
                      "{\\"name\\":\\"Accel X\\",\\"uniq_id\\":\\"cc35x1_ax\\",\\"stat_t\\":\\"cc35x1/ax\\",\\"unit_of_meas\\":\\"raw\\",\\"dev\\":{\\"ids\\":[\\"cc35x1\\"],\\"name\\":\\"LP-EM-CC35X1\\"}}",
                      "{\\"name\\":\\"Accel Y\\",\\"uniq_id\\":\\"cc35x1_ay\\",\\"stat_t\\":\\"cc35x1/ay\\",\\"unit_of_meas\\":\\"raw\\",\\"dev\\":{\\"ids\\":[\\"cc35x1\\"],\\"name\\":\\"LP-EM-CC35X1\\"}}",
                      "{\\"name\\":\\"Accel Z\\",\\"uniq_id\\":\\"cc35x1_az\\",\\"stat_t\\":\\"cc35x1/az\\",\\"unit_of_meas\\":\\"raw\\",\\"dev\\":{\\"ids\\":[\\"cc35x1\\"],\\"name\\":\\"LP-EM-CC35X1\\"}}" };
                    for (int k = 0; k < 4; k++) { osi_uSleep(400000); MQTT_IF_publish(hMqttConn, (char*)cfg[k], (char*)pl[k], strlen(pl[k]), MQTT_QOS_0 | 0x100); }
                    UART_PRINT("HA discovery published\\r\\n");
                }
'''
    return t[:j]+disc+t[j:]
edit('mqtt_client_app.c',app)
PY
make -C "$C/freertos/ticlang" SIMPLELINK_WIFI_SDK_INSTALL_DIR="$SDK" SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR="$TB" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CGT" GCC_ARMCOMPILER= > "$RUN/b.log" 2>&1 \
 && echo "ok: $C/freertos/ticlang/mqtt_client.out" || { echo FAILED; grep -m5 -i "error" "$RUN/b.log"; }
