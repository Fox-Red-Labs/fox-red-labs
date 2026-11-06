# LP-EM-CC35X1 thermostat firmware

Terms: [vocab](../../docs/vocab.md). This is TI's `mqtt_client` demo with one added task. It reads text lines from the G3519 on UART0 (header pin 3 RX is GPIO18, pin 4 TX is GPIO17, 115,200 baud, 8N1) and parses the `T,seq,presence,zones,setpoint,temp` lines of [PROTOCOL.md](../PROTOCOL.md). A line that does not parse is dropped.
After each good line it publishes four retained QoS 0 messages to the broker, in order: `thermostat/presence`, `thermostat/setpoint`, `thermostat/temp`, then `thermostat/seq` last.
On every broker connect it also publishes three retained Home Assistant discovery configs. The demo's subscriptions and QoS 2 publishes are gone, so a broker that only speaks QoS 0 and 1 works. `tools/hub.py` is such a broker.

Build (writes only to the output folder the script prints, programs nothing):
`HOME_SSID="<network name>" BROKER_HOST=<Mac IP> BROKER_PORT=1883 ./build.sh`
Set `WIFI_ENV` to a file outside the repository that holds `WIFI_PASSWORD=`. The network name and password go into the firmware copy only, never into this repo.
The script prints the image folder, the flash and RAM sizes, and the exact Toolbox command that would flash it.

What overwrites what: if the board holds the Matter image, programming this image replaces it. The Matter pairing then stops working, and getting it back means programming the Matter image again and pairing it again.
Programming is one command from the Toolbox folder with your probe serial. Nothing in this folder runs it.
To restore TI's own demo afterwards, program the Network Terminal image over it, as in the main cc35x1 README.
