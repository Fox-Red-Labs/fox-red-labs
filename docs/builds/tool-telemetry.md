# Read a machine's run events on a phone, no internet

Bluetooth tags on your machines, a Wi-Fi hub and a phone. Read run events, trips and run time with no internet. Built on an LP-EM-CC2340R5 tag and an LP-EM-CC35X1 hub.

The page: [site/project-tool-telemetry.html](../../site/project-tool-telemetry.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Tag, hub and phone, with the tag's buttons or a simulator as the machine

Two LaunchPad kits and a phone. The recorded run used the simulator at 3 times real speed.

Status: Works. Boards: cc2340r5, cc35x1.

Folders this version uses (in a sparse clone, fetch them first, then read `cc2340r5/AGENTS.md`):

```bash
git sparse-checkout add cc2340r5 mac-setup tool-telemetry
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC2340R5 LaunchPad development kit | 1 per machine | the tag: reads the machine's lines and advertises them over Bluetooth |
| LP-EM-CC35X1 LaunchPad development kit | 1 | the hub: Wi-Fi access point and Bluetooth scanner |
| LP-XDS110ET debugger | 2 | one debug probe for the tag and one for the hub (the README also allows an LP-XDS110 for the tag) |
| A phone with a browser | 1 | joins the hub's Wi-Fi and shows the app |
| A Mac or Linux computer | 1 | builds, flashes and runs the home base program |
| Two jumper wires (only to connect a real motor controller) | 2 | the controller's TX to the tag's RX, and ground; not needed with the buttons or the simulator |
| LP-AM13E230 LaunchPad development kit | optional | a real motor controller with the telemetry firmware; built, not run on a spinning motor |

### Before you start

- A Mac with SimpleLink Low Power F3 SDK 9.21, SimpleLink Wi-Fi SDK 10.20, SysConfig 1.26 and TI Arm Clang 5.1 (`mac-setup/README.md`). The scripts patch a copy of TI's examples and leave the SDKs alone.
- Clone the code with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch this version's folders.

### Steps

1. **Mac, terminal.** Check the data formats and the on-tag model. No hardware needed.

   ```text
   python3 tool-telemetry/proto/test_proto.py
   python3 tool-telemetry/ai/train.py
   python3 tool-telemetry/ai/test_model.py
   ```

   You should see: `OK: 9014 checks, C and Python codecs agree`, the two trees' scores, then `OK: C model matches the Python trees`.

   A missing script means you are not in the repository root.

2. **Mac, terminal.** Build and flash the tag (CC2340R5 on its probe). The flash erases the chip, so save what is on it first.

   ```text
   tool-telemetry/tag/build.sh
   tool-telemetry/tag/flash.sh
   ```

   You should see: `@TAG boot name=Tag XXXX ... ext=0`. `ext=0` means extended advertising started.

   If the flash cannot connect, pull the probe off the board and press it back on.

3. **Mac, terminal.** Build and flash the hub (CC35X1 on its LP-XDS110ET). The first build makes a random Wi-Fi password and saves it in a secrets file outside the repository. The build prints the file name.

   ```text
   tool-telemetry/gateway/build.sh
   tool-telemetry/gateway/flash.sh
   ```

   You should see: `[ttgw] AP FRL-HUB-XXXX up, address 10.0.0.3` and `[ttgw] scanning (rc=0)`.

   If the console stops before `http listening`, press reset once.

4. **Bench, then phone.** Make run events. Hold the tag's left button to run the machine, or press the right button for a jam. Or play a machine from the computer, giving the tag probe's port. On the phone, join Wi-Fi `FRL-HUB-XXXX` with the password from step 3 and open `http://10.0.0.3`.

   ```text
   python3 tool-telemetry/sim/toolsim.py /dev/cu.usbmodem<tag probe>1 --minutes 5 --speed 3
   ```

   You should see: The Tools tab lists the machine within seconds. Tap it for its load mix. Leave the hub's Wi-Fi and the Saved tab keeps everything.

   If the phone cannot reach the hub, the page may still be asking the site address. The app falls back to 10.0.0.3 (not rechecked on a phone).

5. **Mac, browser.** Open the web dashboard on your computer: one file, no install, no internet. With no hub in reach it replays three bench runs (`tool-telemetry/results/e2e-7`, `e2e-5` and `e2e-zephyr`) at 10 times speed for about a minute. Press Replay to rerun, or drag the slider to 7:00 for the moment in the picture. Click a tag to filter its events.

   ```text
   open tool-telemetry/web/dashboard.html
   ```

   You should see: Three asset tags with battery, last event, runs ended, overcurrent trips and a run timeline (over-limit events in red), plus hub status and the latest events, as in the picture. Equipment 03, a shorter run, goes Offline 30 s after its last event. The other two stay Online.

   No `open`? Double-click the file, or type file:// and its full path into a browser. No web server needed.

6. **Computer, browser.** Put the computer on the hub's Wi-Fi (or the site network the hub joined). In Use a live hub at the page bottom, type the hub's address (10.0.0.3 on its own Wi-Fi) and press Connect. The page polls `/api/info`, `/api/tags` and `/api/log` every 2 seconds. It has run only against a stand-in serving a recorded hub's answers. In a narrow window it stacks into one column, as in the picture.

   You should see: Not seen on a running hub: Connected to FRL-HUB-XXXX and the asset tags from step 4 with live counts. The picture shows the narrow layout with demo data.

   If it says `No hub answered at 10.0.0.3. Is this computer on the hub's Wi-Fi?`, the computer is on another network, or the page came from an https address, which a browser will not let call an http hub.

7. **Mac, terminal.** Send it home. Start the home base on the computer with a word as a token. On the phone's Saved tab enter the computer's address and the token, press Upload, then open `http://localhost:8090`.

   ```text
   python3 tool-telemetry/homebase/server.py --host 0.0.0.0 --token <a word you choose>
   ```

   You should see: The fleet page fills with the uploaded records.

   Without `--token`, anyone on your network could upload. Keep it.

8. **Mac, terminal.** Make the dataset and retrain the model on what you collected.

   ```text
   python3 tool-telemetry/ai/dataset.py
   python3 tool-telemetry/ai/train.py --from-gold <work folder>/dataset/gold/pulls.csv
   ```

   You should see: The retrained model lands in `tool-telemetry/ai/out-collected/`. The shipped model stays.

   Needs at least one upload from step 7.

### Expected result

Within seconds the phone lists the machine and counts its run events, trips and run time, and keeps them offline.

### What this shows

- A motor controller's run events reach a phone through a Bluetooth tag and a Wi-Fi hub: 283 of 283 in one simulated run, median 1.4 s.
- Once loaded from the hub, the phone app needs no internet. The home base keeps each record once, even when two paths deliver it.
- A one-file web dashboard shows the same data on a computer, with a timeline, and replays recorded runs with no hardware.

### Limits

- That it works on a real running motor. The AM13E230 firmware is built and answers commands, but no motor has spun under it.
- That the on-tag model is right on real machines. It scored 95.9% against a simulator's own labels.
- Range, battery life or more than two tags.
- That live mode works on the running hub. It ran only against a stand-in, because the hub was not on this network. In the demo, the three tags are three different recorded runs, and hub uptime and frame counts are spread over the replay, not recorded.
- Data on the air is not encrypted. The hub's log lives in RAM. Bench builds leave the hub's model and command endpoints open (`SECURE=1` closes them).
- The CC2340R5 has about 200 bytes of RAM left with models, commands and leases in.
- Checked on one Intel Mac only.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Events reaching the hub | 283 of 283; median 1.4 s, 95% within 5.0 s | `tool-telemetry/README.md` |
| Lines from machine to tag | 1,264 sent, 1,264 good, 0 bad | `tool-telemetry/README.md` |
