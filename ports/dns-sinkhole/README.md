# How do you run a DNS ad blocker on a TI LP-EM-CC35X1?

**Build the firmware, program the board, and point one device at it by hand.** The board answers DNS on UDP port 53. Names on the blocklist get 0.0.0.0, everything else is forwarded to your gateway and the answer is relayed. Nothing changes on the network until you point a device at the board. Status: Works on the board (experiment: [DNS sinkhole on the LP-EM-CC35X1](../../docs/experiments/2026-10-20-cc35x1-dns-sinkhole.md)).

Terms: [vocab](../../docs/vocab.md). Board setup, activation and the programmer: [cc35x1/README.md](../../cc35x1/README.md). 1 kB = 1,024 bytes.

## Build

1. Get a blocklist. StevenBlack/hosts is MIT licensed (its `license.txt`); it merges third-party lists, so read its README for their terms before redistributing the file. Do not commit the list.
   `curl -fsS -o hosts.txt https://raw.githubusercontent.com/StevenBlack/hosts/master/hosts`
2. Make the table: `python3 gen_blocklist.py hosts.txt -o blocklist.bin` (5 bytes per domain, sorted).
3. Build and sign (takes about 2 minutes; the Wi-Fi name comes from the Mac's current network, the password from a file with `WIFI_PASSWORD=`):
   `APP=full BLOCKLIST=$PWD/blocklist.bin WIFI_ENV=<file> ./build.sh`
   `APP=min ./build.sh` builds stage 1, which answers 192.0.2.1 to every A query. Set `UPSTREAM=<ipv4>` to forward somewhere other than the gateway.
4. Program the signed build from the Wi-Fi Toolbox folder (`/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23`), then reset the board and read the console at 115200 baud:
   `./simplelink-wifi-toolbox programmer -i XDS110 programming --tool_settings <build folder>/ex/freertos/ticlang/toolbox/tool_settings.json`
   Add `-param1 <probe serial>` when more than one probe is connected. `flash_run.sh` wraps this command with retries and a console capture; it is written for this repo's probe serial and capture helper, so edit it before use. The network name and password are inside the image you built: keep it out of git.

## Try it

- From the Mac, without changing any setting: `dig @<board ip> doubleclick.net` returns 0.0.0.0; `dig @<board ip> www.python.org` returns the real address.
- Counters: `curl http://<board ip>/`, or the console line every 10 s.
- Point one device at it by hand: set that device's DNS server to the board's address in its network settings. To go back, set it to automatic again (or delete the entry). The board is not inline, so unplugging it only affects devices pointed at it.
- Measure: `python3 measure.py <board ip> <gateway ip> hosts.txt`.

## Results

71,492 domains (349 kB in flash). From a Mac on the same Wi-Fi, 100 queries each: blocked names median 6 ms (p95 20); forwarded names median 70 ms (p95 172) against 52 ms (p95 99) to the gateway directly. Blocker RAM 0.7 kB static. Largest list in the 2,687 kB application slot: about 304,000 domains (link arithmetic).

## Limits

No DNSSEC: answers are relayed as the upstream gave them and are not validated. Safety checks (from a security review on 2026-10-20): the forwarding ID is a full 16-bit random number from the chip's hardware random generator through the SDK's PSA crypto (`psa_generate_random`); if that is unavailable the board does not forward. Each forwarded query leaves from its own UDP socket on a fresh random port (49152 to 65535, from the same generator) and the name is sent in random upper and lower case (0x20); a reply counts only if it arrives on that socket, from the upstream address on source port 53, with the matching ID and a question that equals what was sent letter for letter. The client gets its own spelling back. Queries from outside the board's own subnet are dropped. Queries over 512 bytes and replies over 1.2 kB are dropped. Host test: `cc -Ifirmware tests/test_checks.c -o t && ./t` (subnet and reply-matching functions only; the lwIP path is tested on the board).

Exact name match only (a listed name does not block its subdomains). IPv4 upstream, UDP only, no caching, 16 queries in flight, 2.5 s forward timeout. A 40-bit hash means a name not on the list matches with probability about 7e-8 for 71,492 entries. The board takes its address by DHCP, so reserve it in the router if devices point at it.

## Porting lessons

- lwIP's raw UDP callbacks run on the tcpip thread, so nothing inside them may block. The sender's address is valid only until the callback returns: copy it before deferring a reply.
- A blocklist of 5-byte hashes, sorted, goes into read-only data with `.incbin` and is searched in place in the execute-in-place flash. 71,492 names take 349 kB of flash and 0.7 kB of RAM.
- A forwarding resolver needs its own defences: a random 16-bit ID from the hardware generator, one UDP socket on a fresh random port per query, random upper and lower case in the name, and a reply accepted only on that socket from the upstream address.
- The SDK demo takes the Wi-Fi name and password at compile time. macOS hides the current network name from command-line tools, so read it from the saved-network list, and keep the built image out of git.
- If the programmer fails with error -260 on every call while a BoosterPack is stacked, remove the pack and replug the probe cable.
