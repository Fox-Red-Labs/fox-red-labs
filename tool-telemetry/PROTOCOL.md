# Tool telemetry wire formats: UART, Bluetooth LE, HTTP and signed commands

Terms: [docs/vocab.md](../docs/vocab.md). Four hops, one format each. The reference codec is `proto/tt_proto.py` (host) and `proto/tt_proto.c` (firmware); the test vectors in `proto/vectors.json` are checked against both by `proto/test_proto.py`. Version 1, 2026-10-17.

```
tool MCU --UART--> BLE tag --advertising--> gateway --HTTP (Wi-Fi)--> phone app --HTTPS/HTTP--> home base
                                                    \--UART "modem" line or Wi-Fi uplink--------> home base
```

## 1. Tool MCU to tag: UART lines

115200 baud, 8N1, ASCII, one line per message, ended by `\n` (a `\r` before it is ignored). At most 160 bytes.

```
$TS,n=42,id=UNIT-07,trig=118,oc=3,flt=0,run=5120,rpm=3566,ipk=2150,vb=12400,tc=31,st=1*6C
```

- `$`, a two-letter type, then `key=value` pairs separated by commas, then `*` and two upper-case hex digits: the XOR of every byte between `$` and `*`. A line with a bad checksum is counted (`rx_bad`) and dropped.
- `n` is the sender's line counter (0 to 65535, wraps). The tag counts gaps (`rx_gap`) but accepts the line.
- Unknown keys are ignored. A key that is missing keeps its last value. Numbers are decimal.

| Type | Sent | Keys |
|---|---|---|
| `TS` tool status | every 1 s while the motor runs, every 10 s idle, and at once on any change of `st` | `id` tool name (up to 8 of `A-Z a-z 0-9 -`), `trig` trigger pulls, total (u32), `oc` overcurrent trips, total (u16), `flt` driver faults, total (u16), `run` motor run time, total seconds (u32), `rpm` speed now (u16), `ipk` peak phase current since the last `TS`, mA (u16), `vb` supply, mV (u16), `tc` board temperature, °C (i8), `st` state: 0 idle, 1 running, 2 fault, 3 locked |
| `TB` battery pack | every 10 s, from the pack if it has its own MCU, or from the tool | `soc` state of charge, % (u8), `cyc` charge cycles (u16), `vb` pack voltage, mV (u16), `tc` pack temperature, °C (i8) |
| `TE` event | when it happens | `ev` = `trig` (a trigger pull ended; `v` = pull length, ms, capped at 65535), `oc` (overcurrent trip; `v` = peak current, mA), `flt` (driver fault; `v` = fault code), `boot` (tool MCU reset; `v` = reset cause) |
| `TP` pull summary | when the trigger is released, after the `TE trig` line | `dur` pull length, ms, `ravg` / `rmax` average and peak speed, rpm, `iavg` / `ipk` average and peak phase current, mA (0 if the tool cannot measure it), `vmin` lowest supply during the pull, mV, `tc` temperature at the end, °C, `cls` optional load class if the tool classified it itself (0 light, 1 normal, 2 heavy, 3 jam). The tag numbers each pull and, when `cls` is absent, classifies it with `ai/out/tt_model.c` |
| `TC` command, tag to tool | stage two: phone or gateway asks the tool to act | `cmd` = `stop`, `lock`, `unlock`, `run` with `duty` (percent, 0 to 40) |

Totals live in the tool MCU (it knows the truth and survives a tag swap); the tag keeps the last values and its own event log.

## 2. Tag to air: Bluetooth LE advertising

Every tag runs two advertising sets at once.

**Set 1, legacy, connectable and scannable, every 500 ms.** Any phone or scanner sees it, Bluetooth 4 included. 25 of 31 bytes:

| Bytes | Field |
|---|---|
| `02 01 06` | Flags: LE general discoverable, no BR/EDR |
| `15 FF` | Manufacturer specific data, 21 bytes follow |
| `FF FF` | Company ID 0xFFFF (the value the Bluetooth SIG reserves for tests; a product would use its own ID) |
| `46 52` | Magic "FR" |
| `11` | Version 1 (high nibble), frame type 1 = legacy status (low nibble) |
| 1 byte | `seq`, the tag's snapshot counter (wraps); a scanner logs a new record when it changes |
| 3 bytes | `trig`, little-endian u24 |
| 2 bytes | `oc`, u16 |
| 2 bytes | `flt`, u16 |
| 4 bytes | `run`, seconds, u32 |
| 1 byte | `flags`: bit 0 running, 1 fault active, 2 battery low (under 20 %), 3 tool link alive (a line in the last 15 s), 4 battery data present, 5 extended set running |
| 1 byte | `soc`, % (0xFF if no battery data) |
| 1 byte | `tc`, °C, signed |

The scan response carries the name, `Tag XXXX` (the last two bytes of the tag's address).

**Set 2, extended, non-connectable, non-scannable, every 1 s, 1M primary and secondary PHY.** Needs a Bluetooth 5 scanner (the gateway). Flags, the name, and one manufacturer data field holding `FF FF 46 52 12 <seq>` and then type-length-value blocks:

| Type | Length | Contents (little-endian) |
|---|---|---|
| 0x01 counters | 12 | `trig` u32, `oc` u16, `flt` u16, `run` u32 |
| 0x02 live | 10 | `rpm` u16, `rpmx` u16 (peak since the last snapshot), `ipk` u16 mA, `vb` u16 mV, `tc` i8, `st` u8 |
| 0x03 battery | 6 | `soc` u8, `cyc` u16, `vb` u16 mV, `tc` i8 (absent if no `TB` line was ever seen) |
| 0x04 tool id | 1 to 8 | ASCII |
| 0x05 events | 9 per event, up to 8 (the tag sends 5), newest first | `id` u16 (the tag's running event number), `type` u8 (1 trigger, 2 overcurrent, 3 fault, 4 tool boot, 5 tag boot, 6 model installed: `v` is its version), `t` u32 (tag uptime in seconds when it happened), `v` u16 |
| 0x07 pulls | 16 per pull, up to 3, newest first | `id` u16 (the tag's pull number), `dur` u16 in 10 ms units, `ravg` u16, `rmax` u16, `iavg` u16 mA, `ipk` u16 mA, `vmin` u16 mV, `tc` i8, `cls` u8 (255 not classified) |
| 0x06 tag | 12 (10 from tag firmware 1) | `up` u32 tag uptime s, `link` u16 seconds since the last good line (0xFFFF never), `rx_ok` u16, `rx_bad` u8, `fw` u8, `mv` u16 the model version the tag runs (0 the built-in one); readers take 10 or more bytes |

A full frame (name, 5 events, 3 pulls, battery, tool id, tag health) is up to 216 bytes, inside one AUX_ADV_IND (no chaining).

Nothing is encrypted in version 1. Anyone nearby can read the counters; a product would encrypt the manufacturer data with a per-tag key (Hubble's format does this; see DESIGN.md).

## 3. Gateway to phone: HTTP over the gateway's Wi-Fi

The gateway is an access point (`FRL-HUB-XXXX`, WPA2, address 10.0.0.3, the address TI's network_terminal gives its access point) and, when configured, also a station on a site network. Plain HTTP, JSON, port 80.

| Request | Answer |
|---|---|
| `GET /` | The phone app (one HTML file) |
| `GET /api/info` | `{"heap","heapmin","hub","fw","up","unix","tags","recs","first","scan":{"adv","ours","legacy","ext","dropped"}}`; `up` is gateway uptime in seconds, `unix` 0 until a phone or SNTP set the clock |
| `GET /api/tags` | One object per tag seen: `addr`, `name`, `rssi`, `age` (s since last heard), `nl`/`ne` (legacy and extended frames heard), and every field from section 2 by its key (`tool`, `trig`, `oc`, `flt`, `run`, `rpm`, `rpmx`, `ipk`, `vb`, `tc`, `st`, `flags`, `soc`, `cyc`, `bvb`, `btc`, `tup`, `link`, `rxok`, `rxbad`, `fw`, `mv`) |
| `GET /api/log?since=N` | `{"next":M,"more":0|1,"items":[...]}`, at most 100 items a page. Items are snapshots (`"k":"s"`, written when a tag's `seq` changes), events (`"k":"e"`, one per new event `id`, deduplicated per tag) model installs (`"k":"m"`: `ver`, `ok`, `err`, `ms`; section 5) and pull summaries (`"k":"p"`: `pid`, `dur` ms, `ravg`, `rmax`, `iavg`, `ipk`, `vmin`, `tc`, `cls`; a jump in `pid` counts as missed pulls, reported per tag as `pmiss` in `/api/tags`). Each has `i` (log index), `a` (tag address), `up` (gateway uptime s) and `unix` (0 if the clock was not set) |
| `POST /api/time` body `unix=1760700000` | Sets the gateway clock from the phone. Answer `{"ok":1}` |
| `POST /api/model` body: one model package as hex | Section 5. Answer `{"ok":1,"kind":0,"ver":2,"ready":0|1}`; `ready` once both trees of one version are in |
| (all POSTs to `/api/model`, `/api/cmd`, `/api/lease`) | A hub built with `SECURE=1` requires header `X-Token` with its control token (401 otherwise); bench builds leave them open. `GET /api/info` adds `"site":{"lat","lon"}` when the hub was built with `SITE_LAT` and `SITE_LON` |
| `GET /api/model` | `{"ver":[v0,v1],"ready","state","tags","on_version","installs_ok","installs_failed","last":{"ok","err"}}`; `state` 0 idle, 1 to 6 an install in progress |

The log is a ring of 2,048 items in RAM. `since` older than the oldest item returns from the oldest, with `"lost":K`.

## 4. To the home base

Everything arrives at one endpoint, from either path:

```
POST /api/ingest   {"via":"phone|modem|wifi","hub":"FRL-HUB-1A2B","items":[...],"tags":[...]}
```

`items` are section 3 log items; the phone adds `unix` to any item that lacked it, from its own clock (`phone_now - (hub_up - item_up)`). The home base keeps one row per (`hub`, `a`, `k`, `seq` or event `id`), so the same item arriving by phone and by modem is stored once.

**The hub's own uploads.** Built with `UPLINK_URL=http://<home base address>:8090/api/ingest` (and `UPLINK_TOKEN`), the hub posts its new log items itself every 30 seconds, 50 at a time, faster while it is behind, as `{"via":"hub","hub":..,"hub_up":<its uptime>,"items":[..],"tags":[..]}`, over whatever network it joined as a station: the site's Wi-Fi, a phone's hotspot (the phone's cellular link) or a cellular router. The home base dates items that lack `unix` from `hub_up`. `GET /api/info` reports `"uplink":{"sent","posts","failed","behind"}`. IP addresses only for now (no DNS on the hub).

**Modem line.** The gateway also prints every new log item on its UART as `@L {json}` and its info as `@I {json}` every 30 s. On the bench that UART is the LaunchPad kit's USB port and `homebase/modem.py` forwards the lines; in a product it is the UART to a cellular modem, which is how most modems attach.

## 5. Home base to tag: installing a model

A model goes the other way: trained at the home base, carried to the site, installed on each tag by the hub.

**Package** (`ai/model_pkg.py`, `proto/tt_mdl.h`). One decision tree: `kind` u8 (0 uses current, 1 speed only), `ver` u16, `n` u8 nodes (1 to 40), `crc` u16 (CRC-16/CCITT over kind, ver, n and the nodes), then `n` nodes of 5 bytes (`f` i8 feature, negative for a leaf; `thr` u16, the class at a leaf; `l` u8, `r` u8 child indexes, always after the node), then a 16-byte MAC: HMAC-SHA256 over kind, ver, n and the nodes with the fleet key, cut to 16 bytes. A model is two packages of one version. `model-v<N>.json` holds both as hex with the version, scores and sources; the home base serves the newest at `GET /api/model`.

**To the hub.** `POST /api/model` with each package (`homebase/pushmodel.py` does it; so can a phone that fetched the model). The hub needs no key: it only carries the package.

**Hub to tag.** GATT service 0xFAB0 on the tag's connectable legacy set. Characteristic 0xFAB1 (write with response, 20 bytes at the default MTU):

| Write | Bytes | The tag |
|---|---|---|
| BEGIN | `01` kind ver(2) n crc(2) | stops using that tree (the built-in one answers meanwhile) |
| DATA | `02` offset(2) up to 17 node bytes | takes chunks in order, no gaps |
| COMMIT | `03` mac(16) | checks length, CRC and tree shape at once, then the MAC on its own task; a pass goes live and into flash, a failure brings back the last good model from flash |

Errors come back as ATT application errors 0x81 (op), 0x82 (length), 0x83 (state), 0x84 (CRC), 0x85 (tree). Characteristic 0xFAB2 (read, 8 bytes): `v0` u16, `v1` u16, live bits (bit 0 tree 0, bit 1 tree 1), result (0 ok, 6 MAC refused, 7 still checking), bytes received u16. The hub reads it until the result is final, logs a `"k":"m"` item, hangs up and scans again. It installs on one tag at a time, only on tags that report tag firmware 2 or later and another version, three tries a tag, and backs off 30 s after a success so the tag can advertise its new `mv`.

The tag checks every installed model again at boot, and an upload left open for 30 s is dropped. Anyone can start an upload, but only a package with the fleet's MAC ever runs. The key lives outside the repository (`<scratch>/secrets/tool-telemetry-model.key`, made by `tag/build.sh`) and is built into the tag; a product would give each fleet its own key.

## 7. Home base to tool: signed commands (stage two)

The home base signs a command for one tag; the hub carries it; the tag checks it and passes it to the tool MCU as a `TC` line (section 1); the tool acts and reports its state (`st=3` locked) in its next `TS` line.

**Command**, 18 bytes: `cmd` u8 (1 stop, 2 lock, 3 unlock, 4 run), `duty` u8 (run only, percent 0 to 40), `counter` u32, `mac` 12 bytes = HMAC-SHA256(fleet key, `C0` || the tag's address, 6 bytes as advertised || cmd duty counter), cut to 12. The `C0` byte keeps command MACs apart from model MACs; the address binds a command to one tag; the counter must be larger than the last one the tag accepted (kept in flash), so a recorded command never works twice. `homebase/sendcmd.py` uses seconds since 1970, never less than its last.

**Home base to hub**: `POST /api/cmd` body `addr=40:F3:B0:xx:xx:xx&cmd=<36 hex digits>`; `GET /api/cmd` gives `{"pending","last":{"cmd","ok","err"}}`. One command waits at a time; it goes before any model install, to that tag only, when the hub has heard it in the last 5 s.

**Hub to tag**: characteristic 0xFAB3 (write, 18 bytes), then 0xFAB4 (read, 5 bytes: last counter u32, result: 0 passed to the tool, 1 bad MAC, 2 old counter, 3 not a command, 7 still checking). The hub logs `"k":"c"` (`cmd`, `duty`, `ok`, `err`, `ms`); the tag logs event 7 (`v` = cmd | duty << 8) only for a command it passed on.

**Leases (command 5, argument = minutes, 0 ends the rule).** A lease keeps the tool usable for that long; with no newer lease by then, the tag sends `$TC,cmd=lock` itself and logs event 7 with `v` = 2 | 255 << 8. A newer lease unlocks it. The rule and the locked state are kept in flash: after a restart a locked tool locks again at once, and an unlocked one gets one minute to hear a hub, so restarting a taken tool does not keep it usable. The hub's lease endpoint takes no token: anyone on its network can stop or replace a tool's leases (the tool then locks; it cannot be unlocked without a signed lease). The home base signs a batch (`homebase/leases.py`); the hub holds up to 20 per tool and hands one over every `every` seconds while it hears the tool: `POST /api/lease` body `addr=..&every=40&cmds=<36 hex digits per lease>`, or `addr=..&stop=1`; `GET /api/cmd` lists them. A hub cut off from the home base keeps a site running for count x every seconds, and a stolen hub no longer than that.

## 6. Tag to Hubble: 13 bytes

Hubble Network carries 0, 4, 9 or 13 bytes a message (its SDK's `HUBBLE_BLE_MAX_DATA_LEN` is 13) and adds the device identity and a sequence number itself. The tag's message (`proto/hubble_payload.py`):

| Bytes | Field |
|---|---|
| 0 | layout (high 4 bits, 1) and flags: fault now, jam or overcurrent since the last report, battery low, used in the last 24 h |
| 1 to 3 | trigger pulls, lifetime, 24 bits |
| 4 to 5 | motor run time, lifetime, tenths of an hour |
| 6 | overcurrent trips, lifetime, stops at 255 |
| 7 | battery charge, percent (255 unknown) |
| 8 | battery health, percent of design capacity (255 unknown) |
| 9 to 12 | last site, latitude and longitude, 16 bits each (about 300 m by 450 m at 40 degrees north); 0x8000 unknown |

The 9-byte form drops the position; the 4-byte form is flags and pulls. Example, bench run 7 with a site set: `1a1301000200095bff3e3c79cd` (275 pulls, 0.2 h, 9 trips, 91% charge, 42.359 N 71.056 W).
