# A DNS ad blocker on a Wi-Fi board

The LP-EM-CC35X1 answers DNS on your network. Names on a 71,492-name blocklist get 0.0.0.0; everything else is forwarded to your router. Nothing changes until you point one device at it.

The page: [site/project-dns-ad-blocker.html](../../site/project-dns-ad-blocker.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## The blocker on the LP-EM-CC35X1

Make the blocklist table, build the firmware with your Wi-Fi name inside, program the board and ask it with dig.

Status: Works. Boards: cc35x1.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/dns-sinkhole/AGENTS.md`):

```bash
git sparse-checkout add ports/dns-sinkhole cc35x1 tools mac-setup
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC35X1 LaunchPad development kit | 1 | the Wi-Fi board that answers DNS |
| LP-XDS110ET debugger | 1 | programs the board |

### Before you start

- A Mac with TI Arm Clang 5.1.1.LTS, the SimpleLink Wi-Fi SDK 10.20.00.39 and the Wi-Fi Toolbox (`mac-setup/README.md`), and a 2.4 GHz Wi-Fi network the board can join.
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add ports/dns-sinkhole cc35x1 tools mac-setup`. Every command on this page runs from the repository folder.
- The blocklist is StevenBlack/hosts (MIT). It merges third-party lists: read its README for their terms before redistributing the file. Do not commit it.
- The Wi-Fi name and password are compiled into the image. Keep the built image out of git. Back up the board first with the command in `cc35x1/AGENTS.md`.

### Steps

1. **Mac, terminal.** Download the hosts file.

   ```text
   cd ports/dns-sinkhole
   curl -fsS -o hosts.txt https://raw.githubusercontent.com/StevenBlack/hosts/master/hosts
   ```

   You should see: `hosts.txt` exists and lists tens of thousands of names.

   If curl fails, check the network and run it again.

2. **Mac, terminal.** Hash and sort the names: 5 bytes per domain.

   ```text
   python3 gen_blocklist.py hosts.txt -o blocklist.bin
   ```

   You should see: `blocklist.bin` is about 349 kB.

   If Python reports a syntax error, use Python 3.12 or newer (`.python-version`).

3. **Mac, terminal.** Build the full blocker. The Wi-Fi name comes from the Mac's saved networks and the password from a file you name. It takes about two minutes. `APP=min ./build.sh` builds stage 1, which answers 192.0.2.1 to every name.

   ```text
   APP=full BLOCKLIST=$PWD/blocklist.bin WIFI_ENV=<file with WIFI_PASSWORD=...> ./build.sh
   ```

   You should see: The script prints the build folder with `ex/freertos/ticlang/toolbox/tool_settings.json`.

   If it cannot read the network name, macOS hides it from command-line tools: set it in the file you pass as `WIFI_ENV`.

4. **Mac, terminal.** Program the signed build with the Wi-Fi Toolbox, then reset the board. Add `-param1 <probe serial>` when more than one probe is connected.

   ```text
   /Applications/ti/simplelink_wifi_toolbox_macos_4_3_23/simplelink-wifi-toolbox programmer -i XDS110 programming --tool_settings <build folder>/ex/freertos/ticlang/toolbox/tool_settings.json
   ```

   You should see: The programmer reports success; the console at 115200 baud shows the board's address.

   If the programmer fails with error -260 on every call, remove any BoosterPack and replug the probe cable.

5. **Mac, terminal.** Ask for a listed name and an unlisted one. Your Mac's settings do not change.

   ```text
   dig @<board ip> doubleclick.net
   dig @<board ip> www.python.org
   ```

   You should see: The first answer is 0.0.0.0. The second is the real address.

   If dig times out, check the board joined your network: the console prints its address, and a router may hand it a new one, so reserve it.

6. **Mac, browser.** Set one device's DNS server to the board's address in its network settings. To go back, set it to automatic. The board is not inline, so unplugging it only affects devices pointed at it.

   ```text
   curl http://<board ip>/
   ```

   You should see: The counters page shows blocked and forwarded names rising as that device browses.

   If the device loses the network, set its DNS back to automatic.

### Expected result

The board answers 0.0.0.0 for blocklisted names and the real address for the rest.

### What this shows

- A 71,492-name blocklist fits the board as a sorted table of 5-byte hashes searched in place in flash: 349 kB of flash and 0.7 kB of RAM.
- A blocked name is answered in a median 6 ms.

### Limits

- No DNSSEC: answers are relayed as the upstream gave them, not validated.
- Nobody ran it as a household's only resolver for days.
- Exact name match only: a listed name does not block its subdomains.
- IPv4 and UDP only, no caching, 16 queries in flight, a 2.5 s forward timeout.
- A 40-bit hash means an unlisted name matches with a probability of about 7e-8. The largest list the application slot takes is about 304,000 names (link arithmetic, not booted).

### Numbers and sources

| | Value | Source |
|---|---|---|
| Blocked name | median 6 ms, 95th percentile 20 ms | `ports/dns-sinkhole/README.md` |
| Forwarded name | median 70 ms, 95th percentile 172 ms (router directly: 52 and 99) | `ports/dns-sinkhole/README.md` |
| Forwarding defences | random 16-bit ID from the hardware generator, a fresh random UDP port per query, random upper and lower case in the name | `ports/dns-sinkhole/README.md` |
