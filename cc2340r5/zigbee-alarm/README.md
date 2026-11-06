# How do I make a TI CC2340R5 report smoke and CO to Home Assistant over Zigbee?

Terms: ZHA, ZBT-2, end device, router, IAS Zone, NVS and CIE are defined in [docs/vocab.md](../../docs/vocab.md) (CIE is the alarm panel that receives the alert; here, Home Assistant).

Build this folder with one command, flash it, open joining in ZHA, and the board shows up as one device with a **Smoke** and a **Carbon monoxide** binary sensor, no custom code on the Home Assistant side. It listens on its USB serial port for the lines `SMOKE 1`, `SMOKE 0`, `CO 1`, `CO 0` and sets or clears the zone's alarm bit. Result on 2026-10-13: the device was listed 5.6 s after the flash finished, the sensors came up with device classes `smoke` and `carbon_monoxide`, and 20 of 20 changes landed for each zone (median 38 ms for smoke and 56 ms for CO). Status: Works, one run. Problems and fixes are in the table below and in [FAQ.md](../FAQ.md).

## Ten-minute path

1. Tools (versions used): TI Arm Clang 5.1.1, SysConfig 1.26.3, the SimpleLink Low Power F3 SDK 9.21.00.36, UniFlash 9.6.0, at the paths in `build.sh`.
2. `./build.sh` (about 30 s). The image is `freertos/ticlang/alarm.hex`. Add `EXTRA_CFLAGS=-DFRL_DEBUG_UART` for status lines on the serial port (`ZB <signal> <status>`, `NTF <endpoint> <status>`).
3. In Home Assistant: Settings, Devices, Zigbee Home Automation, Add device (or the `zha.permit` service, duration 240).
4. `/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c ../CC2340R5.ccxml -e -f -u freertos/ticlang/alarm.hex` (13 s). The flash erases the board's stored network, so it joins as new every time.
5. The device appears in ZHA about 6 s later as "Fox Red Labs FRL-ALARM". Send a line: `python3 uart_send.py /dev/cu.usbmodem<probe serial>1 "SMOKE 1"` and the Smoke sensor turns on.

## What it is

TI's `onoff_light` Zigbee example (ZBOSS stack, FreeRTOS) with the light replaced by two IAS Zone servers: endpoint 1 zone type 0x0028 (fire sensor), endpoint 2 zone type 0x002B (CO sensor; ZBOSS spells the same value `GAS_SENSOR`). The role is end device with the receiver on. The serial lines arrive through UART2 on the debug probe's back channel (115200, 8N1) in callback mode; the code is `alarm.c` (about 300 lines), the configuration `alarm.syscfg`. Logging is off (RAM).

The zone status change notification is sent by this project's own code. ZBOSS's `zb_zcl_ias_zone_set_status()` only notifies when an alarm bit gets set; clearing it returns false and sends nothing, so Home Assistant stayed on "smoke" after `SMOKE 0`. `alarm.c` sets the attribute and sends the notification itself to the address ZHA wrote.

## If you see ... do

| You see | Do |
|---|---|
| Device joins but the sensors never change | Home Assistant did not enroll the zone (zone state 0). Remove the device in ZHA, wait until it is gone, open permit-join, flash again. One of three scripted re-pairings never showed up within two minutes after a removal that was only seconds old (cause not isolated; suspected: pairing before ZHA finished the removal). |
| `IAS zone` entities with no device class | Same cause: the zone type was not read. Reconfigure the device in ZHA (or remove and re-pair). |
| The board goes silent after a minute | You built with `zb_sleep_now()` enabled or the sleepy power mode. Both stop the receiver and the serial listener; see below. |
| The board stops answering when ZHA writes the CIE address | Use the extended zone attribute list (`ZB_ZCL_DECLARE_IAS_ZONE_ATTRIB_LIST_EXT`). With the short list the board stopped responding at that write; with the extended one it did not (cause inside the closed library not checked, suspected: the short list has no internal context entry). |

## Sleepy: tried, does not work with a serial input

The syscfg value `zigbee.powerModeOperation = "sleepy"` plus TI's `zb_sleep_now()` (the default in the examples) built and joined, and 10 of 10 changes landed right after the join. After an idle spell the board stopped answering ZHA's reads and stopped hearing serial lines. Cause not isolated (suspected: the UART receiver is off in standby; a product would wake the chip with a GPIO line from the listener, then read the UART). So the image keeps the receiver on. Current was not measured; no battery figure is claimed.

## Files

`alarm.c`, `alarm.syscfg`, `build.sh`, `freertos/ticlang/makefile` and `lpf3_zigbee_freertos.cmd` (TI's, renamed), `osif/` and `dmm/` (TI's, unchanged). Bench tools: `uart_send.py`, `uart_cap.py`, `latency.py` (the 20-event measurement), `join_bench.sh`, `ha_ws.py` (Home Assistant websocket helper; the token is read at run time and never printed).

Parts of `alarm.c`, `osif/` and `dmm/` are from TI's SDK example (BSD-3-Clause, Texas Instruments Incorporated).
