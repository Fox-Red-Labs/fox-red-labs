# How do you test a TI LP-EM-CC35X1 as an MQTT sensor from a Mac?

**TI's `mqtt_client` example already answers MQTT requests, so a 30-line Python script on the Mac can time it.** Publish to `cc35xx/GetSensors` and the board replies on `cc35xx/Sensors` with the board's temperature and accelerometer readings, so no button press is needed.

Terms: [docs/vocab.md](../../docs/vocab.md). Board guide: [cc35x1/README.md](../README.md). Results: [RESULTS-cc35x1.md](../RESULTS-cc35x1.md). Checked 2026-10-09 with Wi-Fi SDK 10.20.00.39, a Mosquitto 2 broker with no login, and Python 3 with `paho-mqtt` 2.

1. Copy `examples/rtos/LP_EM_CC35X1/demos/mqtt_client` from the SDK to a folder outside this repo.
2. In the copy, set your network name and password in `wifi_settings.h` (`AP_SSID`, `AP_PASSWORD`) and the broker in `mqtt_settings.h` (`MQTT_CONNECTION_ADDRESS`, `MQTT_CONNECTION_PORT_NUMBER`). The password ends up in the firmware you program, so keep the copy out of git and program TI's Network Terminal over it when you are done.
3. Build and program it as in the README's ten-minute path (step 4).
4. On boot the serial port shows `MQTT EVENT: CONNACK` and four `SUBACK` lines.
5. Run `request_reply.py` (set the broker address at its top): it publishes a request every few seconds and prints how many replies came back and how long each took.

The example's README says client login is not implemented, so the broker must allow anonymous clients. Use a throwaway broker on a trusted network.
