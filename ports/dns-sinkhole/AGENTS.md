# What should an AI assistant know before changing the DNS sinkhole on the LP-EM-CC35X1?

Terms: [vocab](../../docs/vocab.md). Build with `build.sh`; flash with the Toolbox command in `README.md` (or `flash_run.sh` after editing its probe serial and capture path). Facts below were checked 2026-10-20 with Wi-Fi SDK 10.20.00.39, Toolbox 4.3.23, TI Arm Clang 5.1.1.LTS.

- The firmware builds inside a copy of the SDK's `demos/mqtt_client` folder; `dns_sinkhole.c` is copied over `mqtt_client_app.c`. The Wi-Fi name and password go into the copy's `wifi_settings.h` only. Never commit a built image or a network name.
- The hash is 64-bit FNV-1a over the lower-case dotted name, folded to 40 bits: `(h ^ (h >> 40)) & 0xFFFFFFFFFF`. `gen_blocklist.py` and `dns_sinkhole.c` must stay identical; check with `python3 gen_blocklist.py --check blocklist.bin <name>` against a `dig` result.
- Raw lwIP UDP callbacks run on the tcpip thread. Start sockets from `mainThread` with `tcpip_callback`, never from the Wi-Fi event callback.
- The table is linked with `.incbin` into `.rodata` (flash, execute in place). The application slot is 2,687 kB; the table must fit beside about 1.2 MB of code.
- Do not change the router, DHCP or any device's DNS. Test with `dig @<board ip>` only.
- Programming error -260 on every Toolbox call means the probe will not open the target; with a BoosterPack stacked, remove it first.
- Sources of truth: `docs/experiments/2026-10-20-cc35x1-dns-sinkhole.md` for numbers, `RESULTS-ROWS.csv` for rows (both ship with this repository).
