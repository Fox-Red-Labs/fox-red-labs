<!-- experiment
id: E236
board: LP-EM-CC35X1
date: 2026-10-20
status: works
reached: verified
line: A DNS sinkhole with 71,492 blocked domains as a 349 kB hash table in flash answers blocked names in a median 6 ms and forwarded names in a median 70 ms against 52 ms asking the gateway directly, from the LP-EM-CC35X1.
stop: none; the largest list is link arithmetic (about 304,000 domains), not boot-tested
-->
# Can a TI LP-EM-CC35X1 act as a DNS ad blocker for the devices pointed at it?

**Short answer.** Yes. Stage 1 (fixed answer) worked first time: `dig @<board> test.example` returned 192.0.2.1 in 4 ms (the first query after idle took 674 ms). The full sinkhole blocks names on a 71,492-domain list (answer 0.0.0.0) and forwards the rest to the gateway: blocked median 6 ms (p95 20 ms), forwarded median 70 ms (p95 172 ms) against 52 ms (p95 99 ms) asking the gateway directly, 100 queries each, from a Mac on the same Wi-Fi. Ten real names returned the same addresses as the gateway. Programming was delayed about 25 minutes by probe error -260 while a BoosterPack was stacked on the LaunchPad; it cleared after the pack was removed and the probe cable was replugged.

Terms: [vocab](../vocab.md) (DNS, sinkhole, lwIP). FAQ: [LP-EM-CC35X1](../../cc35x1/FAQ.md). Board guide: [cc35x1/README.md](../../cc35x1/README.md). Code: [ports/dns-sinkhole](../../ports/dns-sinkhole/README.md). 1 kB = 1,024 bytes.

**Question.** Pi-hole style blocking needs a Linux box. A small DNS server that keeps its blocklist as sorted hashes in flash can run on a microcontroller. Does the CC35X1 answer fast enough, and how large a list fits?

**Vendor position.** TI's Wi-Fi SDK 10.20.00.39 ships lwIP with the raw UDP API and sockets (`lwip_iperf_udp_server.c` uses `udp_bind`). No DNS server example ships.

**What was tried.** Stage 1 (`dns_min.c`): bind UDP 53 and answer every A query with 192.0.2.1. Stage 2 (`dns_sinkhole.c`): parse the query, hash the name (64-bit FNV-1a folded to 40 bits), binary-search 5-byte entries in flash, answer 0.0.0.0 for a hit, otherwise forward to the upstream and relay the answer; counters on the console and at `http://<board>/`. The list is StevenBlack/hosts (MIT license, read from its `license.txt` and the GitHub license field on 2026-10-20), 71,492 names on 2026-10-18; the table is built by `gen_blocklist.py` and is not committed.

**Expectations, written before any run, and results** (100 queries each; three repeats gave 6, 7 and 6 ms median for blocked names).

| Item | Predicted | Measured |
|---|---|---|
| Stage 1: `dig @<board> test.example` | NOERROR, one A record 192.0.2.1, 10 to 40 ms | Yes: NOERROR, 192.0.2.1, 4 ms (first query after idle 674 ms) |
| Blocked name, median and p95 | 15 to 30 ms median | 6 ms median, 20 ms p95 (faster: no upstream hop and a short Wi-Fi round trip) |
| Allowed name through the board | under 10 ms more than the gateway alone, p95 under 80 ms | 70 ms median, 172 ms p95 against 52 and 99 ms to the gateway: 18 ms more at the median (wrong; the extra is a second Wi-Fi round trip plus the copy) |
| Table size | 5 bytes per domain | 349 kB for 71,492 (5.00 bytes each), about 49 kB per 10,000 |
| RAM of the blocker's own code | under 20 kB | 0.7 kB static (link map); packet buffers come from the shared lwIP pool |

The allowed names were made-up names under example.org, so the gateway resolved each with an upstream lookup (negative answers are not cached: the second pass cost the same). Counters after the three runs: 1,034 queries, 402 blocked, 632 forwarded, 631 relayed, 1 timeout, no drops, no bad replies. The queries came only from the Mac, so the outside-subnet drop was not exercised on the board; its subnet function has a host test.

**Flash budget (measured at build).** The Toolbox flash map gives the application image 2,687 kB (`FLASH_NON_SECURE`, 0x29FC00) of the 8 MB external flash. The image with the 71,492-name table links at 1,550 kB, so code and TI's drivers take 1,201 kB and the rest, 1,486 kB, would hold 304,298 domains at 5 bytes each. This is a link-time figure, not a boot-tested one. The whole image uses 482 kB of RAM (mostly lwIP's pools), so the blocker's share is small.

**How far it got.** Verified on the board: stage 1, then stage 2 with the 71,492-name table, forwarding, relay and the status page. After a security review, each forwarded query uses its own socket on a random port with a random-case name (0x20), and replies are accepted only on that socket from the upstream on port 53 with a matching ID and an exact-case question; the numbers above were re-checked after the change (blocked median 6 ms, forwarded 67 to 76 ms) from the chip's hardware generator, queries from outside the board's subnet are dropped, and relayed replies are capped at 1.2 kB.

**The line.** No limit reached on the board. The ceiling of about 304,000 domains is the application slot minus the code, not a boot test. The probe error -260 (20:30 to 21:00, a BoosterPack stacked, `xds_open` failing on every Toolbox call, including a read-only fuse check) is covered in [BoosterPack at plug-in](2026-10-20-boosterpack-on-at-plug-in.md). Whether the pack caused it is not confirmed. macOS redacts the Wi-Fi name, so the first image used a wrong name and the join timed out; the name came from the Mac's saved-network list.

**To retry.** Remove any stacked BoosterPack, then run the commands in the port README.
