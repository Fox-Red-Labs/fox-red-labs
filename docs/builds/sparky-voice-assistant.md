# A voice assistant on a LaunchPad

A LaunchPad with the Audio BoosterPack hears you, asks a language model through a server on your network and speaks the answer. Add the 3.5 inch touch panel for four animated looks and offline kids' games.

The page: [site/project-sparky-voice-assistant.html](../../site/project-sparky-voice-assistant.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Voice assistant, with or without the screen

Run a xiaozhi server, build and program the image, join Wi-Fi over Bluetooth, press a key and ask.

Status: Works. Boards: cc35x1.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/xiaozhi/AGENTS.md`):

```bash
git sparse-checkout add ports/xiaozhi cc35x1
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC35X1 LaunchPad development kit | 1 | the board |
| LP-XDS110ET debugger | 1 | programs the board and gives it a serial console |
| BOOSTXL-AUDIO audio BoosterPack plug-in module | 1 | microphone, DAC and speaker |
| Waveshare 3.5 inch capacitive touch LCD (ST7796S screen, FT6336U touch) | 1 | optional: the screen. Without it the assistant is voice only; with it you get the four looks, touch settings and the kids' games |
| A computer on the same network that runs Docker | 1 | runs the xiaozhi server, the speech and text services and the language model call |

### Before you start

- The code is in this repository. Clone it first: `git clone --depth 1 --filter=blob:none https://github.com/Fox-Red-Labs/fox-red-labs.git`. This page calls the folder you clone it into `<repository folder>`. Every command below runs from inside it.
- TI's free tools: the CC35X1 SDK, SysConfig, TI Arm Clang and the SimpleLink Wi-Fi Toolbox, each from ti.com; `python3 tools/frl/frl.py doctor --board cc35x1` lists which are missing and where to get them. Opus, TFLite Micro and the other libraries are in the repository.
- A language model the server can call. This build used Claude Haiku through the relay in `ports/xiaozhi/host/`, which caps calls at 8 a minute and 150 a day; a local model through the server's own config works the same way.

### Wiring

Only for the optional screen. Header pins are the LP-EM-CC35X1's (SDK board file and the user's guide). LCD_DC and LCD_RST avoid header pins 25 and 26, which reach the chip through ADC buffers on this LaunchPad. Pin 30 also runs to the audio pack's J5 jumper: keep J5 on SPI DAC. None of these pins is used by the audio pack.

| Module pin | Board pin | What it is | Needed |
|---|---|---|---|
| VCC | header pin 1 (3V3) | 3.3 V supply | required |
| GND | header pin 20 (GND) | ground | required |
| SCLK | header pin 4 (GPIO17) | SPI clock, 10 MHz | required |
| MOSI | header pin 38 (GPIO19) | SPI data to the panel | required |
| MISO | header pin 3 (GPIO18) | reads the panel ID back at start | optional |
| LCD_CS | header pin 37 (GPIO16) | chip select | required |
| LCD_DC | header pin 18 (GPIO26) | data or command | required |
| LCD_RST | header pin 30 (GPIO31) | panel reset | required |
| LCD_BL | header pin 8 (GPIO14) | backlight on | required |
| TP_SCL | header pin 9 (GPIO33) | touch clock | required |
| TP_SDA | header pin 10 (GPIO32) | touch data | required |
| TP_INT | header pin 11 (GPIO15) | low while a finger is down | required |
| TP_RST | header pin 14 (GPIO28) | touch reset | required |

### Steps

1. **Computer.** The board does the listening and the speaking; a server on your network turns speech into text, asks the language model and speaks the answer. Run the open-source xiaozhi-esp32-server (MIT license) in Docker on a computer on the same Wi-Fi, with a speech-to-text and a text-to-speech provider in its config. This build used FunASR, Edge TTS and Claude Haiku through a small rate-limited relay (`ports/xiaozhi/host/claude_relay.py`, which keeps the API key out of the server's config). Note the computer's IPv4 address.

   You should see: The server listens on port 8000, path `/xiaozhi/v1/`.

   If the board later says No server, check that the computer's firewall allows port 8000 from the Wi-Fi.

2. **Bench.** Press the BOOSTXL-AUDIO onto the LaunchPad headers. For the screen, wire the 3.5 inch touch panel as in the wiring picture and the wire table above; keep the pack's J5 jumper on its default SPI DAC position.

   You should see: The pack sits flat on the headers. No wire is needed for voice alone.

   If you skip the screen, nothing else changes: the image runs the same without a panel attached.

3. **Terminal.** Clone the repository, put your Wi-Fi name and password in a small file (`HOME_SSID=...` and `WIFI_PASSWORD=...`) outside the repository, and build with the server's address. The values go into the firmware copy under `out/`, never into the repository. `XZ_WAKE=0` builds push-to-talk (see the limits for the wake word). The build needs 1.5 GB of free disk and flashes nothing.

   ```text
   XZ_WAKE=0 XZ_HOST=<server IPv4> WIFI_ENV=<your wifi file> ports/xiaozhi/target-cc35x1/build.sh
   ```

   You should see: `out/xiaozhi-cc35x1/b/xiaozhi.out` and a `b/toolbox/` folder of signed images. The image is about 1.6 MB of the board's 2.7 MB application slot.

   If a tool is missing the script names it and its download page; `python3 tools/frl/frl.py doctor --board cc35x1` lists what is missing.

4. **Terminal.** From TI's SimpleLink Wi-Fi Toolbox folder, program the image over the board's debug probe, then reset it with `xds110reset` (the Toolbox does not reset afterwards). Programming takes under a minute. If the board ran another program before, erase it first with `full_flash_erase` as `cc35x1/AGENTS.md` says.

   ```text
   ./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> programming --tool_settings <repository folder>/out/xiaozhi-cc35x1/b/toolbox/tool_settings.json
   xds110reset -s <probe serial>
   ```

   You should see: The console on the probe's serial port (115200 baud) prints a ready line about 6.5 s after reset, once the board has joined Wi-Fi.

   If the board does not answer, it may be in the other flash slot after an over-the-air update: erase, then program again (the FAQ for the board has the exact commands).

5. **Phone, if needed.** If you built without a network, or it cannot join, the blue LED lights and the board advertises as `TI CC3xxx`. Send the network name and password with TI's SimpleLink Connect phone app, or from a computer with `ports/wifi-provisioning-ble/provision.py`. The board saves the network and joins it at every start. Holding S2 for 5 seconds (or typing `W` on the console) forgets it.

   You should see: The board joins and prints its IPv4 address.

   If it never advertises, wait: it tries the saved or built-in network three times first.

6. **Bench.** Press S2, or type `s` on the console, and ask a question about 30 cm from the pack. The microphone stops while the answer plays, so wait for it to finish; one turn per press.

   You should see: Your words are transcribed on the server and the first reply audio starts about 2.5 s after you stop (median 2.47 s, three questions, Claude Haiku).

   If the reply is quiet or distorted, `v` and `V` set the volume (0 to 16, starts at 6); the microphone clips beside the speaker from about 8.

7. **Bench.** Build without `XZ_WAKE=0` and with `XZ_WAKE_MODEL=<your microWakeWord .tflite>` (its `.json` beside it). The board listens by itself, opens the connection only when it hears the phrase, runs one turn, closes it and listens again, with a 2 s deaf period after each reply.

   You should see: The console prints `[wake] ready`; on the screen the word line reads Say "Hey Sparky".

   The Hey Sparky model itself is not published (see the limits), so this step is for your own phrase.

8. **Screen (optional).** Swipe left or right anywhere, or press LOOK in the settings strip (hold a finger still for 1 s to open it), to step Core, HUD, Visor, Kid.

   You should see: An energy orb in a ring: the ring lights with the voice, comets circle while it thinks, a crown of rays rises while it talks.

   If nothing happens on a swipe, check the panel wiring and that the image was built with the screen (the default).

9. **Screen (optional).** Swipe to the next look.

   You should see: A thin-line instrument in cyan on black with one amber accent: a mic level while listening, SPEAKING with a level bar while it talks, mode and wake word at the sides.

   If the look does not change, the swipe was too short or began on a button; swipe across the face.

10. **Screen (optional).** Swipe to the next look.

   You should see: A glass band with two glowing capsule eyes that blink and glance, a voice line under them and a scanline every few seconds.

   If the look does not change, the swipe was too short or began on a button; swipe across the face.

11. **Screen (optional).** Swipe to the next look. Kid raises the wake sensitivity while it is on, and puts a GAMES chip in the top right corner.

   You should see: Core in candy colors with a face on the orb: blinking eyes, a smile, an o while listening and a mouth that opens with the reply, and friendlier words (I'm listening).

   If the look does not change, the swipe was too short or began on a button; swipe across the face.

### Expected result

A press of S2 or the `s` key and a spoken question give a spoken answer, first audio about 2.5 s after you stop. With the screen, a face that listens, thinks and talks in one of four looks.

### What this shows

- A Wi-Fi LaunchPad with a BoosterPack can be a complete voice terminal: microphone to Opus to a server and back, with the Opus encoder and decoder running on the Cortex-M33 in 24 kB and 17.5 kB of RAM.
- Median 2.47 s from the end of a spoken question to the first reply audio (2.28 to 2.67 s, three questions, Claude Haiku).
- Joined Wi-Fi 6.5 s after reset (5.9 to 6.7 s, five boots).
- The four looks, touch navigation, the settings strip, a Wi-Fi motion score and offline kids' games run in the same image over the air-updatable slot.

### Limits

- That S2 works by hand: the press was sent from the console.
- Power use: the radio is kept always on and power was not measured.
- A real room: the questions came from a Mac speaker 30 cm away, in a quiet room.
- The server is a separate computer on your network; the board does not run the speech recognition or the language model.
- Half duplex: the microphone stops while the answer plays, so there is no talking over it, and one turn per connection.
- The Hey Sparky wake word model is not in the repository: its training clips are available for non-commercial use only. Build with `XZ_WAKE=0` for push-to-talk, or train a model of your own for the board's detector (microWakeWord on TFLite Micro).
- Sound is the audio pack's small speaker: it has almost nothing below 500 Hz.
- Plain `ws://` to a computer on the same network; the WebSocket client does not check the server's accept hash.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Voice turn | Opus CELT 16 kHz mono, 60 ms packets, complexity 0, 24 kbit/s; xiaozhi WebSocket protocol v1; server speech-to-text FunASR, text-to-speech Edge TTS |  |
| First reply audio | 2.28 to 2.67 s, median 2.47 s, 3 questions, measured from the end of the Mac's speech to the first audio line on the serial console, Mac clock | `data/results/cc35x1.csv` |
| Image size | 1.5 MB flash, 190 kB static RAM (first voice build, TI Arm Clang 5.1.1.LTS, 160 MHz) | `data/results/cc35x1.csv` |
| Screen | Waveshare 3.5 inch, 480 x 320 wide, SPI at 10 MHz, software I2C touch |  |
