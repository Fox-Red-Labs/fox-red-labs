# Connected equipment, from the work site to the cloud and back: start here

Flash three LaunchPad kits, open a phone, and you have the whole system on a desk: a tool that reports its use, a Bluetooth tag that carries it, a hub that collects every tool on a site, a phone that works with no internet, a server that keeps the data and trains a model, and the way back down: new models and signed commands, including a tool that locks itself when it leaves the site.

Terms: [docs/vocab.md](../docs/vocab.md). How it fits together: [README.md](README.md). Formats: [PROTOCOL.md](PROTOCOL.md). Which tag to pick: [TIERS.md](TIERS.md). The data chain to your cloud: [portability/fielddata/CHAIN.md](../portability/fielddata/CHAIN.md).

## What you need

| Kit | Role | Notes |
|---|---|---|
| LP-EM-CC2340R5 + LP-XDS110 | the tag (low cost) | one per tool |
| LP-EM-CC2755P10 + LP-XDS110 | the tag with Hubble's satellite path, more memory | optional |
| LP-EM-CC35X1 + LP-XDS110ET | the hub (Wi-Fi access point, Bluetooth scanner) | one per site; activate the chip once ([cc35x1/README.md](../cc35x1/README.md)) |
| LP-AM13E230 + DRV8376EVM + a Hall BLDC motor | a real tool MCU | optional: the tag's two buttons or the simulator stand in |
| A Mac or Linux computer | builds, flashes, runs the home base | TI SimpleLink Low Power F3 SDK 9.21, SimpleLink Wi-Fi SDK 10.20, SysConfig 1.26, TI Arm Clang 5.1 ([mac-setup](../mac-setup/README.md)) |

## Passwords and locks

Nothing locks and nothing asks for a password unless you turn it on:

- **Tools never lock by themselves** until you give a tag a lease rule (step 8 below). End it any time with `homebase/leases.py --hub <hub> --tag <addr> --end`, and `sendcmd.py ... unlock` lifts a lock.
- **The hub's control endpoints are open** in a normal build, so the scripts and the app work at once; the tags still refuse anything not signed with your fleet key. Build with `SECURE=1` to require a token (kept in the secrets file and sent by the scripts).
- **The home base takes Lock and Unlock from your own network** without a token. Start it with `--token <a word>` to require one; then enter it in the app's Saved tab.
- **Made for you, never typed:** the fleet signing key and the hub's Wi-Fi password, on the first build. The Wi-Fi password is in `<scratch>/secrets/tool-telemetry-hub.env`.

## Build and flash: one command per board

Run from the repository root. Each script copies TI's example, patches the copy and builds it; the SDKs are never changed. The first build of each makes your own secrets outside the repository, in `<scratch>/secrets/`: the hub's Wi-Fi password and the fleet key that signs models and commands. Nobody else's image works with your tags.

```
tool-telemetry/tag/build.sh && tool-telemetry/tag/flash.sh                      # CC2340R5 tag
tool-telemetry/tag/build.sh cc2755p10 && tool-telemetry/tag/flash.sh cc2755p10  # CC2755P10 tag (set FRL_CC2755_CCXML to your probe's ccxml)
tool-telemetry/gateway/build.sh && tool-telemetry/gateway/flash.sh              # CC35X1 hub
python3 tool-telemetry/homebase/server.py --host 0.0.0.0 --hub-url http://<hub address>   # the home base, on the computer
```

What you should see: the tag prints `@TAG boot name=Tag XXXX ... fw=2`; the hub prints `[ttgw] AP FRL-HUB-XXXX up, address 10.0.0.3` and `[ttgw] scanning (rc=0)`.

To put the hub on your site Wi-Fi as well, and have it upload by itself, build it with three settings (files outside the repository hold the network name and password):

```
SITE_ENV=<file with SITE_SSID=...> WIFI_ENV=<file with WIFI_PASSWORD=...> \
UPLINK_URL=http://<home base address>:8090/api/ingest tool-telemetry/gateway/build.sh
```

Add `SITE_LAT=40.71 SITE_LON=-74.01` (your site, two or three decimals) and the app's header shows the site with a map link. Add `UPLINK_TOKEN=<the word>` when the home base runs with `--token`.
```

The site network can be an office router, a phone hotspot (the phone's cellular link) or a cellular router.

## The demo, in order

Each step lists what to do, what you should see, and what was measured (2026-10-17).

| # | Do | You should see | Measured |
|---|---|---|---|
| 1 | Hold the tag's left button (BTN-1) 3 s, then press the right one (BTN-2) | The hub's page at `http://<hub>` shows BTN-DEMO with pulls and a jam | 9 pulls, 2 jams |
| 2 | Or play a tool: `python3 tool-telemetry/sim/toolsim.py /dev/cu.usbmodem<tag probe>1 --minutes 5 --speed 3` | Pulls, run time, load mix, events within seconds | 283 of 283 events, median 1.4 s |
| 3 | Phone: join `FRL-HUB-XXXX`, open `http://10.0.0.3`; leave the Wi-Fi | The Saved tab keeps everything with no internet | see Limits |
| 4 | Upload from the phone, or let the hub upload by itself | The fleet page at `http://localhost:8090` fills in | 954 records from the hub, 0 failures |
| 5 | Export and train: `portability/fielddata/export.py`, `tool-telemetry/ai/train.py --from-gold ...` | A dataset in open formats and a new model | 95.2% on held-out tools |
| 6 | Send the model down: `ai/model_pkg.py --version 2 ...` then `homebase/pushmodel.py --hub http://<hub> --wait 60` | Every tag reports model v2 in the app; installs in Events | 5 to 6 s an install; forged or older models refused |
| 7 | Lock a tool: the Lock and Unlock buttons on its page in the app (the home base signs), or `homebase/sendcmd.py --hub http://<hub> --tag <addr> lock` | The tool shows Locked, then Idle | 3.6 to 5 s, on the simulator and on the AM13E230 firmware |
| 8 | Leases: `homebase/leases.py --hub http://<hub> --tag <addr> --minutes 1 --every 20`, then `--stop` | With no renewals the tool locks itself; renewals unlock it | locked 58 s after renewals stopped, unlocked 4 s after they resumed |
| 9 | A second tag with a second tool | Both tools tracked, totals exact | 2 tags, totals exact |
| 10 | Hubble (CC2755P10, Hubble account): the board outside during a pass, then `homebase/hubble_sync.py` | The asset message (pulls, hours, battery, last site) on the fleet page under "Where Hubble heard them", with where it was heard | ground path decoded on the bench; the fleet section tested with an example reply; satellite not confirmed |

## Make it yours

- **Your tool.** Any MCU that prints the UART lines in [PROTOCOL.md](PROTOCOL.md) section 1 (`$TS`, `$TE`, `$TP`, 115200 baud, XOR checksum) is a tool; `tool/am13e230/tt_tool.inc` is a worked example. To act on lock, unlock and stop, parse `$TC` lines as `tt_rx()` in that file does.
- **Not a tool at all.** Describe your device in one JSON file (`portability/fielddata/apps/`, copy `presence.json`), have it send `$FD` lines, and the tag, the hub, the app and the export carry it unchanged.
- **Your model.** Train anything that yields a small decision tree (or use `ai/train.py`), package it with `ai/model_pkg.py`, and install it with `pushmodel.py`. For TI's own trainer, `portability/fielddata/tensorlab.py` writes a Tiny ML Tensorlab config and the Edge AI Studio dataset zip.
- **Your cloud.** The export writes Croissant, Frictionless and Hugging Face files beside a dated data lake; [CHAIN.md](../portability/fielddata/CHAIN.md) shows the upload commands for S3 and Hugging Face.
- **Your keys.** Delete the files in `<scratch>/secrets/` named in the build scripts and rebuild every board to make new ones. Keep them out of any repository.

## Known limits

- **Phone on the hub's own Wi-Fi:** a phone that opened the app on the site network and then joined the hub's network reported "no hub in reach" (2026-10-17): the page kept asking the site address. The app now falls back to 10.0.0.3;
- **Firmware updates reset the tag's counters:** a new tag image that changes its flash record layout starts the command counter and model floor again, so older signed commands and models could be replayed once after such an update. Keep the record layout when updating, or move it forward.
- **Security:** bench builds leave the hub's model, command and lease endpoints open (`SECURE=1` closes them), and uploads are plain HTTP; every tag checks every signature, so the realistic risk is disruption, not control. All tags share one fleet key; per-tag keys or public-key signatures fit on the CC2755P10.
- **The CC2340R5 is nearly full:** about 200 bytes of RAM left with models, commands and leases in. The CC2755P10 has about 76 kB.
- **Real motor:** telemetry and lock/unlock run on the AM13E230 firmware, but its Hall learn tripped the motor driver's fault on our board's current setup, so the motor did not spin as the tool.
- **Hub boards:** after programming, the CC35X1 starts on its own; a probe reset afterwards can leave it silent until it is programmed again (`gateway/flash.sh` no longer resets it).
- **Cellular link:** a hotspot or cellular router through the hub's Wi-Fi. A modem on the hub's UART is not built.
