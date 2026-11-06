# How do I test the LP-EM-CC35X1 Matter thermostat with Apple Home, Google Home, Alexa and Home Assistant?

**Add the board to each of the four in turn from one phone, then time six things: setup, a setpoint change from the app, radar presence in the app, control with the internet cable pulled, all four at once, and setup again after a reflash.** This is the test plan. The image it tests is built (see [README.md](README.md), section "Thermostat image"). Apple Home paired the thermostat image on 2026-10-11 and changed the heating setpoint (not timed). The combined Matter and Wi-Fi presence image paired on 2026-10-12; a setpoint write was not seen on it. Google Home, Alexa and Home Assistant are not measured. Results are rows in `data/results/cc35x1.csv`. Vendor pages read 2026-10-10.

Terms: [docs/vocab.md](../../docs/vocab.md). Board page: [cc35x1/README.md](../README.md). Wire format from the radar side: [thermostat-system/PROTOCOL.md](../../thermostat-system/PROTOCOL.md).

## What the device is

Matter device on Wi-Fi, test identity (vendor `0xFFF1`, product `0x8001`, passcode `20202021`, manual code `3497-011-2332`, QR `MT:-24J0GSY00KA0648G00`). Endpoint 1 is a Thermostat (heating and cooling setpoints, local temperature, system mode). Endpoint 2 is an Occupancy Sensing sensor, driven by the radar's presence line. With no radar line the temperature is a fixed 20.8 C and SW1 and SW2 step the heating setpoint by 0.5 C, so the device can be set up and exercised on the bench. The board holds up to 5 controllers at once (the Matter minimum; Home Assistant's page says "at least 5 different fabrics").

The identity is a test one, so no home platform treats it as a certified product. That changes the first step for Google (a developer project is needed) and adds a warning step for Apple. Amazon's pages do not say what Alexa does with a test vendor ID, so record what the app says.

## What each platform needs, and the exact setup steps

| | Apple Home | Google Home | Alexa | Home Assistant |
|---|---|---|---|---|
| Hub or controller hardware | iPhone or iPad on iOS 18 or later can add Matter accessories without a hub; a home hub (Apple TV 4K, HomePod, HomePod mini) is recommended and needed for control away from the phone | A Matter hub: Nest Hub (1st or 2nd gen), Nest Hub Max, Nest Audio, Nest Mini, Google Home speakers, Google TV Streamer (4K), Nest Wifi Pro. Wi-Fi Matter devices need no Thread border router | A Matter-capable Echo or eero (Echo, Echo Dot, Echo Show, Echo Hub and others since the 2nd generation; eero 6 and later). The Echo is the Matter controller | The Matter Server app next to Home Assistant. Home Assistant OS is the supported install; a container install is "at your own risk" |
| Phone | iPhone, Bluetooth on | Android 8.1 or later with Google Play services 22.48.14 or later, or iPhone on iOS 16.5 or later; Bluetooth LE 4.2 or later | Alexa app on a phone with Bluetooth | Home Assistant Companion app on the phone (Home Assistant does not use the server's own Bluetooth) |
| Network | Phone and hub on the same Wi-Fi | IPv6 on the router, or setup can look fine and control then fails | Board within about 9 m of the Echo | Matter Server and board on the same LAN, IPv6 |
| Test-identity step | Tap "Add Anyway" on the uncertified-accessory prompt | Create a project in the Google Home Developer Console with a Matter integration, vendor `0xFFF1`, product `0x8001`, device type Thermostat; restart the hubs; the person adding must be in the project | Unknown: record what the app says | None found; commission from the Companion app |
| Device types the platform lists | Thermostat and sensors (motion, light, contact, temperature, humidity); occupancy shows as a motion sensor, to be confirmed | Thermostat and Occupancy Sensor are both listed | Thermostat is listed; occupancy as a motion sensor is expected, not confirmed from the page | Thermostat and Occupancy Sensing entities |

### Apple Home (Apple Support 102135, published 2026-05-07)

1. Update the iPhone. Open Home, tap Add, then Add Accessory.
2. Scan the QR (`setup-qr.png`) or tap More options and enter `3497-011-2332`.
3. Tap Add Anyway on the uncertified prompt, pick a room, tap Done.
4. Check the thermostat tile (temperature, setpoint) and the occupancy tile.

### Google Home (support.google.com/googlehome/answer/13127223 and developers.home.google.com/matter/get-started)

1. Once, in a browser: console.home.google.com, new project, add a Matter integration with vendor `0xFFF1`, product `0x8001`, device type Thermostat. Restart the hubs.
2. From Apple Home, press and hold the tile, Settings, Turn On Pairing Mode; or reset the board and use the original code.
3. Google Home app, +, Set up device, Matter-enabled device; scan the QR or enter the code; choose home and room.
4. Check the thermostat card.

### Alexa (Amazon help page nodeId TSWevY1sX8ADH2LC20 and developer.amazon.com/en-US/docs/alexa/smarthome/matter-support.html)

1. Alexa app, Devices, plus, Add Device, the Matter icon in the shortcuts bar, Yes.
2. Scan the QR or enter the pairing code. Amazon names two ways in: the original code (the board must be in its pairing window, which a factory reset opens) or a fresh code from the first controller (Apple's "Turn On Pairing Mode").
3. Check the thermostat in the Alexa app and ask an Echo for the temperature.

### Home Assistant (home-assistant.io/integrations/matter, read 2026-10-10)

1. Matter Server app running, Matter integration added (`ws://localhost:5580/ws`). If Home Assistant runs in Docker with no Matter Server, add one: it is a permanent service on the Home Assistant host.
2. Phone path: Companion app, Settings, Devices and services, Add device, Add Matter device, scan the QR.
3. Share path (no Bluetooth): get a pairing code from Apple Home or Google Home and enter it in Home Assistant's "Add Matter device".

## The six measurements

Each becomes rows in `data/results/cc35x1.csv`. Clocks: the Mac's clock for everything the Mac sees; the board's own log lines carry a millisecond tick (`t=`) and the Mac stamps each log line as it arrives on the application UART, so the two can be lined up. Timed by hand or screen recording where only a person can see the result; the note says so.

| # | What | How | Test id and variant (rows) | Unit |
|---|---|---|---|---|
| 1 | Setup time, per platform | From the QR accepted to the device showing in the app with a live temperature. Board log gives the Bluetooth connect and `Commissioning complete` stamps; phone screen recording gives the app side | `radio_join`, variant `matter-thermostat-commission-apple-home`, `...-google-home`, `...-alexa`, `...-home-assistant` | s |
| 2 | Setpoint change, app to board | Change the setpoint in the app; board prints `CONTROLLER_WRITE t=<ms>`; Mac stamps the line. Phone video stamps the tap. Median of 10, 2 s apart, both Wi-Fi bands noted | `radio_cmd_latency`, variant `matter-thermostat-setpoint-<platform>` | ms |
| 3 | Presence, radar to app | Needs the 4-wire radar connection. Walk in; the radar frame number in the `LINE_RX` log is the start, the app tile change on a phone recording is the end. Median of 10 | `radio_cmd_latency`, variant `matter-presence-radar-to-<platform>` | ms |
| 4 | Local control with the internet cable pulled | Unplug the router's WAN cable; change the setpoint from each app that has a hub in the house; record yes or no and the latency | `local_control_wan_down`, variant `<platform>` | yes/no |
| 5 | Four controllers at once | Add the board to all four; change the setpoint from each, check the other three follow; record how many hold the board | `multi_admin_count` | controllers |
| 6 | Setup again after a reflash | Flash the image again, factory reset (SW1 held), time the repeat setup per platform and note which platforms keep a stale tile | `repair_after_reflash`, variant `<platform>` | s |

Also recorded from the build and the first boot (not needing a phone): flash and RAM of the image (`host_fit`), and reset to Bluetooth advertising (`ble_on_air`, variant `matter-thermostat`).

## What you need

- An iPhone (Apple, and the phone for the others), a Google hub for Google Home, an Echo for Alexa, and a Matter Server for Home Assistant. Apple Home needs a home hub for control away from the phone.
- Measurement 3 needs the radar wired to the board's UART.
- The Google Developer Console project needs a Google account.
