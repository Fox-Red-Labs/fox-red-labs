# Asset tracking over Bluetooth with LP-EM-CC2340R5 and LP-EM-CC35X1: every machine on a phone with no internet

A tool's motor controller sends trigger pulls, run time, overcurrent trips and a summary of every pull over a UART to a Bluetooth tag (LP-EM-CC2340R5). The tag classifies each pull with a small model (light, normal, heavy, jam) and advertises everything. A Wi-Fi and Bluetooth hub (LP-EM-CC35X1) hears every tool nearby, and a phone that joins the hub's Wi-Fi gets the whole history in an app the hub serves itself. The phone keeps it and uploads it to a home base later, where it becomes a training dataset.

**Trying it yourself:** [GETTING-STARTED.md](GETTING-STARTED.md) (parts, one command per board, the demo in ten steps, how to make it yours, known limits).

This is also the reference system for the field-data framework in [portability/fielddata/](../portability/fielddata/README.md): any device that sends numbered fields (`$FD` lines) through the same tag and hub shows up in the same app, is stored the same way, and exports to TI's Tiny ML Tensorlab, Edge Impulse and `tools/dataset/`. Room presence is the second app.

Terms: [docs/vocab.md](../docs/vocab.md). Wire formats: [PROTOCOL.md](PROTOCOL.md). Why it is built this way: [DESIGN.md](DESIGN.md).

## How it fits together

```
 tool MCU ──UART──▶ tag ──Bluetooth LE──▶ hub ──Wi-Fi──▶ phone app ──later──▶ home base ──▶ dataset ──▶ model
 (AM13E230 +       (CC2340R5: classifies  (CC35X1: scans,   (served by    (one row per     (bronze,     (C, back
  DRV8376EVM,       each pull, legacy +    keeps the log,    the hub,      record, from     silver,      into the
  or the simulator, extended advertising)  access point +    stores on     every path)      gold)        tag)
  or the tag's                             site Wi-Fi)       the phone)
  own buttons)                                 └──UART "modem" line and site Wi-Fi ──▶ home base
```

| Piece | What it does | Files |
|---|---|---|
| Tool MCU | A status line every second while running, an event per pull end or trip, and a pull summary (length, speed, current, voltage, temperature) | `tool/am13e230/` (the BLDC motor firmware plus telemetry), `sim/toolsim.py` (a simulated unit) |
| Tag | Parses the lines, keeps the last 8 events in flash, classifies each pull, advertises a 25-byte legacy frame any phone hears and an extended frame (up to 216 bytes) with everything. With no tool attached, its left button is the trigger and its right button a jam | `tag/` |
| Hub | Scans Bluetooth all the time, keeps 2,048 records, runs access point `FRL-HUB-XXXX` for phones and joins the site network, serves the app and a JSON API, prints every record on its UART for a modem | `gateway/` |
| Phone app | Live tools, events, each tool's trend and load mix; saves everything on the phone; exports CSV or JSON; uploads to the home base; every view has a link (`#tool=<address>`, `#events`, `#saved`) | `app/index.html` |
| Web dashboard | The phone app's data on a computer: each asset tag's battery, last event, runs, trips and a run timeline, over-limit events and hub status. Reads a hub live, or replays the recorded bench runs with no hardware (`open web/dashboard.html`); one file, no requests except to the hub address you give it | `web/` (`dashboard.src.html`, `make_demo.py`, `build.py`) |
| Home base | Takes uploads from phones, the hub's modem line and the hub's site Wi-Fi; keeps the raw uploads and each record once; shows the fleet | `homebase/` |
| AI | Builds the dataset (raw, clean, training table, schema, datasheet), trains the pull classifier in plain Python and writes it as C | `ai/` |

## Parts

| Part | Qty | Notes |
|---|---|---|
| LP-EM-CC2340R5 | 1 per tool | The tag. Needs an XDS110 debug probe to program (LP-XDS110 or LP-XDS110ET). The LP-EM-CC2755P10 builds from the same files (`tag/build.sh cc2755p10`) and is the one for Hubble's satellite path |
| LP-EM-CC35X1 | 1 | The hub. Plugs onto an LP-XDS110ET; needs its own USB-C cable for power. The chip must be activated once (see [cc35x1/README.md](../cc35x1/README.md)) |
| LP-AM13E230 with DRV8376EVM and a Hall-sensor BLDC motor | optional | A real tool MCU. Without it, use the tag's buttons or `sim/toolsim.py` |
| Jumper wires | 2 | Tool TX to tag RX, and ground, once you move off the USB bridge |

## Two boards and a phone

Everything below runs from the repository root on a Mac with TI's SimpleLink Low Power F3 SDK 9.21, SimpleLink Wi-Fi SDK 10.20, SysConfig and TI Arm Clang installed ([mac-setup](../mac-setup/README.md)). The scripts copy TI's examples to `<scratch>/runs/tool-telemetry/` and patch the copy; the SDKs are never changed.

1. **Check the formats and the model** (terminal):

   ```
   python3 tool-telemetry/proto/test_proto.py
   python3 tool-telemetry/ai/train.py
   python3 tool-telemetry/ai/test_model.py
   ```

   You should see `OK: 9014 checks, C and Python codecs agree`, the two trees' scores, and `OK: C model matches the Python trees`.

2. **Build and flash the tag** (terminal; the CC2340R5 on its probe). The flash erases the chip, so save what is on it first if you want it back.

   ```
   tool-telemetry/tag/build.sh
   tool-telemetry/tag/flash.sh
   ```

   You should see `@TAG boot name=Tag XXXX ... ext=0`; `ext=0` means the extended advertising set started.

3. **Build and flash the hub** (terminal; the CC35X1 on its LP-XDS110ET). The first build makes a random Wi-Fi password for the hub and keeps it in `<scratch>/secrets/tool-telemetry-hub.env`.

   ```
   tool-telemetry/gateway/build.sh
   tool-telemetry/gateway/flash.sh
   ```

   You should see `[ttgw] AP FRL-HUB-XXXX up, address 10.0.0.3` and `[ttgw] scanning (rc=0)`. If the console stops before `http listening`, press the board's reset once. To also join a site network, build with `SITE_ENV=<file with SITE_SSID=...>` and `WIFI_ENV=<file with WIFI_PASSWORD=...>`.

4. **Use it** (bench, then phone). Hold the tag's left button: the "tool" runs. Press the right button: a jam. On the phone, join Wi-Fi `FRL-HUB-XXXX` with the password from step 3 and open `http://10.0.0.3`. The Tools tab shows the tool within a few seconds; tap it for its load mix. Leave the hub's Wi-Fi: the Saved tab still has everything.

5. **Send it home** (computer, then phone):

   ```
   python3 tool-telemetry/homebase/server.py --host 0.0.0.0 --token <a word you choose>
   ```

   On its own the home base listens only to the computer it runs on; `--host 0.0.0.0` lets phones on your network reach it, and the token keeps strangers from uploading. On the phone's Saved tab enter the computer's address and the token, press Upload, then open `http://localhost:8090` for the fleet page. The two scripts below read the token from `FRL_HOMEBASE_TOKEN`. Two more paths reach the same place: `homebase/hubsync.py <hub address> http://localhost:8090` reads a hub on the site network, and `homebase/modem.py <hub serial port> http://localhost:8090` forwards the hub's modem line.

6. **Make the dataset and retrain** (terminal):

   ```
   python3 tool-telemetry/ai/dataset.py
   python3 tool-telemetry/ai/train.py --from-gold <scratch>/runs/tool-telemetry/dataset/gold/pulls.csv
   ```

   The retrained model goes to `ai/out-collected/` (the shipped model in `ai/out/` stays as it is).

**A real tool MCU instead of buttons.** `tool-telemetry/tool/am13e230/build.sh --flash` puts the telemetry on the AM13E230 motor firmware. Until the wire is in, `sim/bridge.py <AM13 port> <tag port>` copies its `$T` lines to the tag. **The simulator** plays a motor-driven unit: `sim/toolsim.py /dev/cu.usbmodem<tag probe>1 --minutes 5 --speed 3`.

## Wiring the tool MCU to the tag

One wire replaces the USB bridge: the tool's UART TX to the tag's UART RX, plus ground. Remove the tag's RXD jumper to its probe first, so the probe does not drive the same pin. Both run at 115200. The exact header pins for the AM13E230 and the CC2340R5 LaunchPad kits are in the user guides: check them there before the wire goes in.

## The round trip: a new model from the home base to every tag

Train at the home base, sign it, hand it to the hub, and the hub installs it on every tool tag it hears:

```
python3 tool-telemetry/ai/train.py --from-gold <export>/tool-telemetry/gold.csv     # or any trainer that writes model.json
python3 tool-telemetry/ai/model_pkg.py --model tool-telemetry/ai/out-collected/model.json --version 2 \
    --out <scratch>/runs/tool-telemetry/models                                    # signs with the fleet key
python3 tool-telemetry/homebase/pushmodel.py --hub http://<hub address> --wait 60   # home base to hub to tags
```

The tag checks the signature before it uses a model, keeps it through restarts, and reports its version in every extended frame; the phone app shows it under the tool, and each install appears in Events. Formats: PROTOCOL.md section 5.

**Commands back to the tool (stage two).** The same path carries a signed command to one tool:

```
python3 tool-telemetry/homebase/sendcmd.py --hub http://<hub address> --tag 40:F3:B0:xx:xx:xx lock     # or unlock, stop, run --duty 20
```

The tag accepts it only with the fleet's signature, for its own address, with a counter larger than the last; it passes a `$TC` line to the tool MCU, and the tool reports its new state. PROTOCOL.md section 7.

**A tool that leaves the site locks itself.** `homebase/leases.py --hub <hub> --tag <addr> --minutes 1 --every 20` gives the hub signed leases to hand the tool while it is in range. On the bench, when the hub stopped renewing, the tool locked itself 58 s later and the app showed it Locked; when renewals came back it unlocked in 4 s. No network is needed for the lock: the tag decides.

**The hub uploads by itself.** Build it with `UPLINK_URL=http://<home base address>:8090/api/ingest` and it posts new records to the home base every 30 s over whatever network it joined: the site's Wi-Fi, a phone's hotspot (the phone's cellular link) or a cellular router.

## Results

Measured on the bench on 2026-10-17: the CC2340R5 tag and the CC35X1 hub about 1 m apart, the hub on a local Wi-Fi network as well as its own access point, the simulated unit at 3 times real speed, 10 minutes a run, the phone app open alongside (`sim/e2e.py`).

| What | Result |
|---|---|
| Codec, C against Python | 9,014 checks, identical bytes |
| Lines from tool to tag | 1,264 sent, 1,264 good, 0 bad |
| Events reaching the hub | 283 of 283; median 1.4 s from the tool's line to the hub's log, 95% within 5.0 s |
| Hub totals against the tool | identical (275 pulls, 9 trips, 661 s run) |
| Pull summaries reaching the hub | 269 of 274; the hub counted the 5 it missed |
| On-tag model, live pulls | 95.9% right against the simulator's truth; 9 of 9 jams caught |
| Model, test tools it never saw | 95.2% with current, 95.3% speed only, 1,200 pulls from 6 tools (model card) |
| Model size | 35 nodes a tree, 6 bytes a node |
| Phone and the hub's web server | one client: 60 of 60 requests, mean 0.11 s; the app plus a second client polling 4 times a second: 17 of about 2,400 requests timed out |
| Paths to the home base | phone, modem line and site Wi-Fi all stored; a record sent twice by two paths was stored once |
| Model update, home base to tag | v2 and v3 installed over Bluetooth by the hub in 4.9 to 6.3 s each; home base to tag 9 s; after the tag was erased the hub reinstalled v2 by itself; installs recorded at the home base |
| Model package checks | the tag's C install code against the trainer's trees and Python's HMAC: 3,136 checks, including a wrong key, a forged MAC, a flipped byte, chunks out of order and a looping tree |

## Limits

Data on the air is not encrypted. The hub keeps its log in RAM. The phone app reloads only on the hub's Wi-Fi. The model has only seen simulated pulls. Commands reach the simulated tool and the real AM13E230 motor firmware: a signed lock made the firmware refuse to run and report locked (state 3) through the tag, and unlock cleared it. The real motor has not spun as the tool: on the board's current setup the six-step firmware's Hall learn trips the DRV8376 fault, which the tag and hub reported as driver faults. A remote run is refused by the motor firmware on purpose. A cellular link is through the hub's Wi-Fi (a phone hotspot or a cellular router): a modem on the hub's UART is not built. Hubble: the ground beacon decoded on the bench, the satellite path did not run. The model install and command endpoints on the hub take no token, and its uploads are plain HTTP; the tags check every signature. Every tag shares one fleet key; per-tag keys or public-key signatures would close that (they fit on the CC2755P10). A command that is never delivered stays valid until a newer one is sent: the tag has no trusted clock for an expiry. The AM13E230 telemetry builds but has not run on the motor. The button demo ran on the bench on 2026-10-17: 9 trigger pulls, 2 jams and 11 pull summaries at the hub. A phone has not been confirmed on the hub's own access point.

## Sources

TI SimpleLink Low Power F3 SDK 9.21.00.36 `basic_ble`; TI SimpleLink Wi-Fi SDK 10.20.00.39 `network_terminal`; Bluetooth Core Specification 5.4.
