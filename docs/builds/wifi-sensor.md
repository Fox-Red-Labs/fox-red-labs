# A Wi-Fi sensor that answers over MQTT

Build TI's MQTT example for the LP-EM-CC35X1, point it at a broker, and time how fast it answers with its temperature and accelerometer. Not connected to Home Assistant.

The page: [site/project-wifi-sensor.html](../../site/project-wifi-sensor.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## TI's MQTT example answering a Mac through a broker, on the LP-EM-CC35X1

The board joins your Wi-Fi, connects to an MQTT broker and replies to a request with its readings. A short Python script on the Mac times the replies.

Status: Works. Boards: cc35x1.

Folders this version uses (in a sparse clone, fetch them first, then read `cc35x1/AGENTS.md`):

```bash
git sparse-checkout add cc35x1
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC35X1 LaunchPad development kit | 1 | the board |
| LP-XDS110ET debugger | 1 | debug probe |
| A computer running an MQTT broker | 1 | the broker the board and the script both talk to |

### Before you start

- TI's SimpleLink Wi-Fi SDK 10.20.00.39 and Wi-Fi Toolbox 4.3.23, installed as in cc35x1/README.md. Checked on an Intel Mac, macOS 15.7.9.
- The board mounted on the debugger and already activated; the README's ten-minute path, steps 1 to 3, does both. Activation burns fuses and cannot be undone.
- Python 3 with `paho-mqtt` 2 on the Mac: `python3 -m pip install paho-mqtt`.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add cc35x1 tools data`. Below, `<repository folder>` is that clone.

### Steps

1. **Computer with the broker.** Start an MQTT broker that accepts clients without a login. Note its address and port.

   You should see: The broker is listening on its port. The test used Mosquitto 2 on port 1883.

   TI's example cannot log in to a broker, so use a throwaway broker on a trusted network, and allow anonymous clients in its settings.

2. **Terminal (Mac).** Copy TI's example from the SDK to a folder outside this repository: `examples/rtos/LP_EM_CC35X1/demos/mqtt_client`.

   ```text
   cp -R /Applications/ti/simplelink_wifi_sdk_10_20_00_39/examples/rtos/LP_EM_CC35X1/demos/mqtt_client ~/mqtt_client
   ```

   You should see: A `mqtt_client` folder in your home folder.

   If the SDK folder is not there, install the Wi-Fi SDK 10.20.00.39 first.

3. **Editor.** In the copy, set your network name and password in `wifi_settings.h` (`AP_SSID`, `AP_PASSWORD`) and the broker in `mqtt_settings.h` (`MQTT_CONNECTION_ADDRESS`, `MQTT_CONNECTION_PORT_NUMBER`).

   You should see: The two files hold your values. The password is now inside the firmware you build.

   Keep this copy out of git, and program TI's Network Terminal over it when you are done.

4. **Terminal (Mac).** Build the copy, sign it with the Toolbox's image builder and program it, as in step 4 of the ten-minute path in `cc35x1/README.md`. `cc35x1/prebuild.sh` has the build and image commands; use absolute paths.

   ```text
   ./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> programming --tool_settings <path to the tool_settings.json the image builder wrote>
   ```

   You should see: The Toolbox reports the write finished.

   If it stops with `Path to programming_instructions after path correction couldn't be found`, rebuild with an absolute build folder (cc35x1/FAQ.md has the fix). If it stops with `Life cycle OPERATIONAL is not valid`, the chip is not activated: do step 3 of the README's path.

5. **Mac, serial port.** Read the board's serial port at 115200 baud, for example with a short Python `termios` script, and reset the board.

   ```text
   /Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xds110reset -s <probe serial>
   ```

   You should see: `MQTT EVENT: CONNACK` and four `SUBACK` lines.

   If the port is silent, read at 115200, the rate of TI's SDK examples; do not use `stty` then `cat` on macOS, it reads at the wrong rate. If there is no CONNACK, check the network name, password and broker address in step 3.

6. **Terminal (Mac).** Set the broker address and port at the top of `request_reply.py`. Then send 20 requests, one every 5 seconds, to the topic the example listens on and wait for the reply topic.

   ```text
   python3 cc35x1/mqtt-test/request_reply.py 20 5 cc35xx/GetSensors cc35xx/Sensors
   ```

   You should see: A line `sent 20 replies 20 loss_pct 0.0`, a line of latencies in milliseconds with a median near 167 ms, and one sample reply holding the board's temperature and accelerometer values.

   If replies are 0, the board and the script are on different brokers or the board never reached CONNACK (step 5). If a few are missing, the Wi-Fi link dropped them: the one-hour run lost none.

### Expected result

The board answers every request with its temperature and accelerometer readings, with a median of about 167 ms through the broker. Nothing reaches Home Assistant in this recipe.

### What this shows

- TI's MQTT example builds from a terminal, joins a home Wi-Fi network and answers 720 of 720 requests over an hour.
- The reply carries the board's temperature and accelerometer values.

### Limits

- That the board appears in Home Assistant. The recorded results list no Home Assistant run, and its MQTT integration was not set up.
- Anything about security: TI's example cannot log in to a broker, so the broker must allow anonymous clients.
- Another board: only the LP-EM-CC35X1 ran this.
- One board, one network, one hour, a broker on a wired Mac mini and the Mac on Wi-Fi. The 167 ms includes the broker and the Mac script, not only the radio.
- Your network password is compiled into the image until you program another image over it.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Topics | request on cc35xx/GetSensors, reply on cc35xx/Sensors | `cc35x1/mqtt-test/README.md` |
| Latency | 167.4 ms median, 1,206.2 ms slowest, 720 of 720 answered, 0.0 percent lost over 3,604 s | `data/results/cc35x1.csv` |
| Software | SimpleLink Wi-Fi SDK 10.20.00.39, Mosquitto 2, Python 3 with paho-mqtt 2 | `cc35x1/mqtt-test/README.md` |
