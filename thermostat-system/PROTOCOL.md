# Thermostat wire protocol: IWRL6432BOOST, LP-MSPM0G3519 and LP-EM-CC35X1

The radar, the LP-MSPM0G3519 and the LP-EM-CC35X1 pass presence, setpoint and temperature in the lines below. Terms: [vocab](../docs/vocab.md). Checked 2026-10-10.

Data path: IWRL6432BOOST radar --(UART, 115,200 baud, 3.3 V, TI's TLV frames as shipped)--> LP-MSPM0G3519 (UART1 RX, PA9)
--(UART, 115,200 baud, ASCII lines, UART1 TX PA8)--> LP-EM-CC35X1 (UART0 RX, GPIO18) --(Wi-Fi, MQTT, QoS 0)--> broker on the Mac --(REST)--> Home Assistant.

## Line from the G3519 to the CC35X1

`T,<seq>,<presence>,<zones>,<setpoint>,<temp>\n`

| Field | Meaning |
|---|---|
| seq | the radar frame number (header field `frameNumber`), decimal; in Mac-bridge test mode the Mac sets it |
| presence | 1 or 0: any zone bit set in TLV 315, else 0 |
| zones | the first payload byte of TLV 315 in decimal (zone count), then `-` and the zone bits byte in decimal, e.g. `1-1` |
| setpoint | setpoint in tenths of a degree C, e.g. 215 is 21.5 C |
| temp | current temperature in tenths of a degree C |

Sent when presence or setpoint changes, and once a second as a heartbeat. The CC35X1 ignores a line that does not parse.

## MQTT from the CC35X1 (QoS 0, no login, broker on the Mac)

State topics (retained): `thermostat/presence` (`ON` or `OFF`), `thermostat/setpoint` (`21.5`), `thermostat/temp` (`20.8`), then `thermostat/seq` (the seq, published last of the four, so its arrival time is the arrival of the whole update).
Discovery (retained, published on connect): `homeassistant/binary_sensor/fxl_therm_presence/config`, `homeassistant/sensor/fxl_therm_setpoint/config`, `homeassistant/sensor/fxl_therm_temp/config` (Home Assistant MQTT discovery JSON; device name "Fox Red Labs thermostat").

## Backchannel lines from the G3519 (UART0 over the XDS110, 115,200)

`SHOWN,<seq>,<ms>` printed right after the frame that drew that seq finished its flush to the panel (`<ms>` is the G3519's own millisecond clock); `STAT,...` once a second with fps, busy percent, heap, stack.
In Mac-bridge test mode the Mac sends the radar bytes on this same UART0 (replay of a real capture), so the same parser runs.
