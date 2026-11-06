#!/bin/bash
# Build the thermostat firmware for the LP-EM-CC35X1: TI's mqtt_client demo plus a UART0 line parser and the
# thermostat MQTT topics (thermostat-system/PROTOCOL.md). Builds in a copy outside the repo. Programs nothing.
#   HOME_SSID="<network name>" WIFI_ENV=<file with WIFI_PASSWORD=...> BROKER_HOST=<ip> BROKER_PORT=<port> ./build.sh
# The network name and password go into the firmware copy only (never into the repo or this script's output).
# Changes to TI's example: mqtt_if.c passes the retain flag (0x100); the demo's subscriptions and sensor publishes are
# removed (everything is QoS 0, will QoS 0, so a QoS 0/1 broker works); a task reads UART0 (GPIO18 RX, GPIO17 TX, 115200 8N1,
# the demo's second UART2 instance CONFIG_UART2_1), parses `T,seq,presence,zones,setpoint,temp` lines and wakes the MQTT loop,
# which publishes presence, setpoint, temp, seq (retained, QoS 0, in that order). Discovery configs go out on every connect.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; RUN="$PWD/out/cc35x1/thermostat"
: "${HOME_SSID:?set HOME_SSID to the Wi-Fi network name}"; : "${WIFI_ENV:=$HOME/.config/frl/home-wifi.env}"
: "${BROKER_HOST:?set BROKER_HOST to the broker IP}"; : "${BROKER_PORT:=1883}"
FREE=$(df -k /System/Volumes/Data | awk 'NR==2{print $4}'); [ "$FREE" -gt 2306867 ] || { echo "disk gate: under 2.2 GB free"; exit 5; }
SDK=/Applications/ti/simplelink_wifi_sdk_10_20_00_39; TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh; CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
rm -rf "$RUN"; mkdir -p "$RUN"; C="$RUN/m"; cp -R "$SDK/examples/rtos/LP_EM_CC35X1/demos/mqtt_client" "$C"
python3 - "$C" "$HOME_SSID" "$WIFI_ENV" "$BROKER_HOST" "$BROKER_PORT" "$HERE/therm_task.c" <<'PY'
import sys
C,ssid,envf,host,port,tfile=sys.argv[1:7]
pw=[l.split('=',1)[1].strip() for l in open(envf) if l.startswith('WIFI_PASSWORD=')][0].strip('"\'')
def edit(p,f):
    t=open(C+'/'+p).read(); t=f(t); open(C+'/'+p,'w').write(t)
def rep(t,a,b,n=1):
    assert t.count(a)>=1, 'patch point missing: '+a[:50]
    return t.replace(a,b) if n==0 else t.replace(a,b,n)
edit('wifi_settings.h',lambda t:rep(rep(t,'"Example_SSID"','"%s"'%ssid),'"Example_PASS"','"%s"'%pw))
edit('mqtt_settings.h',lambda t:rep(rep(rep(t,'"broker.hivemq.com"','"%s"'%host),'MQTT_CONNECTION_PORT_NUMBER     1883','MQTT_CONNECTION_PORT_NUMBER     %s'%port),'#define MQTT_WILL_QOS               MQTT_QOS_2','#define MQTT_WILL_QOS               MQTT_QOS_0'))
old='mqtt_publish(pClient->hLWIPClient, topic, payload, payloadLen, 0, 0, LwipCB_mqttPubAck, pClient);'
edit('mqtt_if.c',lambda t:rep(t,old,'mqtt_publish(pClient->hLWIPClient, topic, payload, payloadLen, 0, (flags & 0x100) ? 1 : 0, LwipCB_mqttPubAck, pClient);'))
edit('freertos/mqtt_client.syscfg',lambda t:rep(t,'UART22.$name = "CONFIG_UART2_1";','UART22.$name = "CONFIG_UART2_1";\nUART22.rxRingBufferSize = 256;'))
tc=open(tfile).read()
disc=open(tfile.replace('therm_task.c','therm_discovery.inc')).read()
def app(t):
    t=rep(t,'#include <ti/drivers/I2C.h>','#include <ti/drivers/I2C.h>\n#include <ti/drivers/UART2.h>\n#include "task.h"')
    t=rep(t,'    APP_MQTT_PUBLISH_SENSORS,\n};','    APP_MQTT_PUBLISH_SENSORS,\n    APP_MQTT_THERM,\n};')
    t=rep(t,'void *mainThread(void *args)',tc+'\nvoid *mainThread(void *args)')
    # start the UART task once the queue exists
    t=rep(t,'    rc = WIFI_IF_init(true);','    xTaskCreate(thermUartTask, "therm", 1024, NULL, 3, NULL);\n\n    rc = WIFI_IF_init(true);')
    # subscriptions out, discovery in
    i=t.index('int ret = MQTT_IF_subscribe(hMqttConn, "Broker/To/cc35xx"'); c='ASSERT_GENERAL(ret == 0);\n'; j=t.index(c,i)+len(c)
    t=t[:i]+disc+t[j:]
    # therm event handler before the connected branch
    k='            else if(queueElement.event == APP_MQTT_CONNECTED)'
    t=rep(t,k,'            else if(queueElement.event == APP_MQTT_THERM)\n            {\n                if (connected) thermPublish(hMqttConn);\n            }\n'+k)
    t=t.replace('MQTT_QOS_2','MQTT_QOS_0')
    return t
edit('mqtt_client_app.c',app)
PY
make -C "$C/freertos/ticlang" SIMPLELINK_WIFI_SDK_INSTALL_DIR="$SDK" SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR="$TB" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CGT" GCC_ARMCOMPILER= > "$RUN/b.log" 2>&1 \
 && echo "ok: $C/freertos/ticlang/mqtt_client.out" || { echo FAILED; grep -m8 -i "error" "$RUN/b.log"; exit 1; }
"$CGT/bin/tiarmsize" "$C/freertos/ticlang/mqtt_client.out" | tee "$RUN/size.txt"
# Toolbox image builder (host side only, burns nothing; same step as cc35x1/prebuild.sh)
d="$C/freertos/ticlang"
make -s -f "$TB/scripts/makefile" all SDK_DIR="$SDK" SYSCONFIG_ARTIFACT="$d" BUILD_DIR="$d" BUILD_ARTIFACT="$d/mqtt_client.out" TOOLBOX_DIR="$TB" > "$RUN/images.log" 2>&1 || true
TS="$C/freertos/ticlang/toolbox/tool_settings.json"; [ -f "$TS" ] || { echo "no tool_settings.json (image step failed), see $RUN/b.log"; exit 1; }
echo "images: $C/freertos/ticlang/toolbox/ (signed with the SDK example key)"
echo "To flash (NOT run here; replaces the Matter image):"
echo "  cd $TB && ./simplelink-wifi-toolbox programmer -i XDS110 -param1 ${PROBE:-<probe serial>} programming --tool_settings $TS --report_file_name_path $RUN/program-report.bin"
