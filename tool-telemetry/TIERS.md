# Two tags, one system: CC2340R5 or CC2755P10

**The short answer.** The low-cost tag (CC2340R5) does everything a work site needs: logging, advertising, the hub, the phone, the home base, model updates and signed commands over the air. The CC2755P10 adds what happens off the site: the satellite path through Hubble, full firmware updates with room to spare, and space for bigger models and stronger security. The tag firmware is the same source for both (`tag/build.sh cc2755p10`).

Terms: [docs/vocab.md](../docs/vocab.md). Measured on the bench on 2026-10-17 unless a row says estimated.

| | LP-EM-CC2340R5 | LP-EM-CC2755P10 |
|---|---|---|
| Flash used by the tag, of total | 198 kB of 500 kB | 215 kB of 916 kB |
| RAM free with the tag running (models and signed commands in) | 257 bytes of 36 kB | 76 kB of 162 kB |
| Radio power | up to 8 dBm | up to 20 dBm |
| Site path: tag, hub, phone, home base | runs (283 of 283 events, median 1.4 s over the radio) | runs (148 of 148 events, median 1.6 s through the hub's API; 141 of 141 pulls) |
| Model updates over the air (signed) | runs: 4.9 to 6.3 s per install, older versions refused | runs: the hub installed v3 on its own when the tag appeared |
| Signed commands to the tool (stage two) | runs: lock and unlock reached the tool in 3.6 to 3.7 s, a replay refused | same code, built |
| Full firmware update over the air | fits with on-chip or off-chip update, not with dual image (it needs 453 bytes of RAM; 257 are left) | fits every scheme TI ships for it, with about 76 kB of RAM to spare |
| Hubble, phones nearby (ground) | possible (Hubble lists the chip for ground use) | possible |
| Hubble, satellite | no: needs 20 dBm, this chip gives 8 without an outside amplifier | yes: Hubble lists this LaunchPad as a validated satellite kit |
| Room for a bigger model | about 40 nodes a tree now; RAM is the limit | hundreds of nodes, or a small neural network |
| Signing a model | a shared fleet key (HMAC) fits | room for public-key signatures, so a tag holds no secret that signs |

## Full firmware updates

TI's SDK ships update examples for both chips. A helper built each one unmodified and read its memory map.

| Chip | How the update works | Room for the application | This repo's tag fits, flash to spare (estimated) | Extra RAM the update needs (measured) |
|---|---|---|---|---|
| CC2340R5 | on-chip: a small resident app receives the new image | 299 kB | yes, about 97 kB | 124 bytes: fits |
| CC2340R5 | dual image: two full copies | 243 kB | yes, about 38 kB | 453 bytes: 28 bytes short |
| CC2340R5 | off-chip: the download goes to the LaunchPad kit's SPI flash | 487 kB | yes, about 275 kB | 295 bytes: fits |
| CC2755P10 | on-chip | 719 kB | yes, about 501 kB | 124 bytes: fits |
| CC2755P10 | dual image | 465 kB | yes, about 244 kB | 453 bytes: fits |

Two things a product must change from TI's examples: the CC2340R5 examples sign images with a key that ships in the SDK (a public demo key), and the CC2755P10 examples sign with an RSA key that ships in the example folder. A product makes its own keys.

So the CC2340R5 can take full firmware updates through the on-chip scheme, or through dual image only by giving up a few hundred bytes this repo's tag uses today (heap or stacks). With models and signed commands in, it has 257 bytes of RAM left: this is about as much as the low-cost tag holds. On the CC2755P10 it is a non-question.

## How much reaches the cloud through Hubble

Hubble moves 13 bytes a message (PROTOCOL.md section 6). The hub path moves everything.

| Path | What one tool sends in a working day | Notes |
|---|---|---|
| Hub, phone, home base | every pull: 16 bytes each, plus events and snapshots. A busy unit at bench-run 7's rate is about 4,400 pulls in 8 hours, about 70 kB of pull summaries (estimated from run 7: 274 pulls in 30 tool-minutes) | everything, in seconds, when a hub is in range |
| Hubble, phones nearby | up to 1,024 messages a day (TI's listing for Hubble's toolkit), 13 bytes each: at most 13 kB, with location from the phone that heard it | only where a phone with Hubble's software passes by |
| Hubble, satellite | a few messages a day: a pass lasts 3 to 5 minutes, one or more times a day, and arrives up to 6 hours later | about 13 to 50 bytes a day (estimated); anywhere with open sky |

What fits in 13 bytes, from this repo's own runs: alive, flags (fault, jam since last report, battery low, used today), lifetime trigger pulls, motor hours, overcurrent trips, battery charge and health, and the last site to about 300 m. Bench run 7 as one message: `1a1301000200095bff3e3c79cd`.

So Hubble answers "where is it, is it working, how much is it used, does its battery need replacing", which are the first questions fleet owners ask. The pull-by-pull data that trains models comes home through the hub. Satellites do not report position, so the message carries the last site, which the tag learns from its hub.

## Limits

- The CC2755P10 ran the site path and a model install on the bench; signed commands and Hubble on it are built, the satellite path is not confirmed.
- Update headroom is estimated from TI's stock examples and this repo's build sizes; the update scheme has not been added to this repo's tag.
- The ground-path message count is TI's listing, not a measurement. The satellite messages-per-pass rate for one LaunchPad with its PCB antenna is not known until the outdoor test.
- The CC2340R5 multistep update example did not build on this Intel Mac (its helper program is an Apple Silicon binary).
