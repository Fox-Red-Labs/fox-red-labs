<!-- experiment
id: E171
board: SK-TDA4VM
date: 2026-10-10
status: line found
reached: verified
line: The TDA4VM's SA2UL crypto block is in the Edge AI kernel and the kernel prefers it (cbc-aes-sa2ul at priority 30,000), but no shipped tool reaches its AES from user space: openssl's afalg engine fails because CONFIG_CRYPTO_USER_API_SKCIPHER is off, and the cryptodev module the image carries has no openssl engine built for it. Driven through /dev/crypto by a 48-line program of this repo, the block does AES-128-CBC at 185 MB/s on 16 kB blocks for 16 percent of one core and 886 MB/s on 64 kB blocks with the core fully busy; the A72's own crypto instructions do 1,490 MB/s. SHA-256 over AF_ALG: 165 MB/s at 16 kB against 969 MB/s on the CPU. The hardware block loses on speed at every block size that could be compared and, through the ioctl path, saves no A72 time per byte for AES either.
stop: no-software
-->

# Does the SK-TDA4VM's SA2UL crypto block beat the Cortex-A72's crypto instructions for bulk AES and SHA on TI's Edge AI Linux?

Terms used here are in [docs/vocab.md](../vocab.md) (SA2UL, AF_ALG and /dev/crypto, ioctl, crypto driver priority, IPsec and dm-crypt) and the board's [FAQ](../../tda4/FAQ.md). Board page: [tda4/README.md](../../tda4/README.md). Numbers: `openssl_speed_mbps` rows in `data/results/tda4.csv`. kB = 1,024 bytes; MB/s = 1,000,000 bytes a second.

**Short answer.** No. On the SK-TDA4VM `openssl speed -engine afalg` fails with `socket bind failed` because the kernel has no user-space cipher interface, and driven through `/dev/crypto` by this repo's own program the SA2UL does AES-128-CBC at 185 MB/s on 16 kB blocks against 1,490 MB/s for the A72's own instructions. It loses on speed at every block size that could be compared (the CPU rate is measured only up to 16 kB) and, through that path, saves no A72 time per byte for AES either (0.86 against 0.67 ms per MB at 16 kB).

**Question.** The TDA4VM has a hardware crypto engine (the SA2UL: AES, SHA, a random number generator) and two Cortex-A72 cores with Arm's crypto instructions. For bulk encryption and hashing from a Linux program, which one is faster, and does the hardware block at least free the CPU?

**Vendor position.** TI, TDA4VM data sheet (read 2026-10-10): the security accelerator "SA2UL" supports AES, 3DES, SHA-1, SHA-2 and HMAC. TI, Processor SDK Linux documentation for J721E, "Hardware crypto accelerator" (read 2026-10-10, written for the non-Edge-AI SDK): the SA2UL driver registers with the kernel's crypto API, and the page shows `openssl speed` numbers obtained through the `cryptodev` module. TI, Edge AI SDK 11.00.00.08 documentation: no mention of the crypto block. TI's documentation for its AM62x chips (a smaller Linux family with the same kind of crypto block), as read for this repo's notes on 2026-10-09 and not re-read today, shows the CPU ahead of the hardware block for bulk AES on that chip (figures not re-read here), which is where the expectation came from: "no for bulk, yes in CPU load".

**What was tried.** Board: SK-TDA4VM (J721EXSKG01EVM), TI Edge AI SDK 11.00.00.08, Linux 6.12.17-ti, openssl 3.2.4, no network, everything over the serial console, 2026-10-10.

1. `openssl speed -engine afalg -evp aes-128-cbc` (2026-10-09): the engine loads and then prints `ALG_PERR: /usr/src/debug/openssl/3.2.4/engines/e_afalg.c(463): Failed to bind socket : No such file or directory`.
2. Why. `/proc/crypto` lists `cbc-aes-sa2ul`, `ecb-aes-sa2ul`, `sha256-sa2ul`, `hmac(sha256-sa2ul)` and two `authenc` combinations, all self-tests passed, AES at priority 30,000 (the CPU's `cbc-aes-ce` is 300). `/proc/config.gz`: `CONFIG_CRYPTO_USER_API_HASH=m`, `CRYPTO_USER_API_SKCIPHER` and `_AEAD` not set, `DM_CRYPT` not set, `CRYPTO_DEV_SA2UL=m`. So user space can open an AF_ALG socket for a hash or the RNG, not for a cipher. The image also loads the out-of-tree `cryptodev` module (`/dev/crypto`, driver 1.14) with its header in `/usr/include/crypto/cryptodev.h`, but this openssl has no `devcrypto` engine (`openssl engine -t` lists only the dynamic loader) and no `cryptsetup` or `kcapi` tools are installed.
3. This repo's own path. `sa2ul_bench.c`: AES-128-CBC through `/dev/crypto` (`CIOCGSESSION`, then one `CIOCCRYPT` per block; `CIOCGSESSINFO` confirmed the kernel chose `cbc-aes-sa2ul`) and SHA-256 through an AF_ALG hash socket; 8 MiB of data at 16 B and 1 kB blocks, 256 MiB at 16 kB and above; pinned to one A72; the process's own CPU time from `getrusage`. `openssl speed` on the same core, in the same run, for the CPU columns (969 MB/s for SHA-256 here; the night before it read 967).

**How far it got.** Measured, on the hardware block and on the CPU.

| Block size | SA2UL AES-128-CBC via /dev/crypto | CPU share while doing it | CPU AES-128-CBC (openssl) | SA2UL SHA-256 via AF_ALG | CPU share | CPU SHA-256 (openssl) |
|---|--:|--:|--:|--:|--:|--:|
| 16 B | 0.8 MB/s | 47% | 314 MB/s | 0.8 MB/s | 48% | 31 MB/s |
| 1 kB | 42 MB/s | 38% | 1,394 MB/s | 41 MB/s | 39% | 673 MB/s |
| 16 kB | 185 MB/s | 16% | 1,490 MB/s | 165 MB/s | 14% | 969 MB/s |
| 64 kB | 886 MB/s | 100% | (16 kB is openssl's largest) | 837 MB/s | 100% | |
| 1 MB | 820 MB/s | 99% | | 808 MB/s | 99% | |

CPU share is the calling process's user plus system time over the run's wall time, on one core. The CPU columns are by definition one core fully busy.

**The line.** Two lines, one in software and one in the hardware's reach.

Software: the stop for a normal user is `CONFIG_CRYPTO_USER_API_SKCIPHER` being off in the Edge AI kernel, with no openssl engine for the `cryptodev` module the image loads anyway. **Tool or docs.** Nothing shipped on the image lets an application use the hardware AES; a program has to talk to `/dev/crypto` itself.

Hardware: the block loses to the A72's crypto instructions on throughput at every block size, by 8 times at 16 kB and still by 1.7 to 1.8 times at 64 kB and 1 MB against the CPU's 16 kB rate (openssl reports no larger block). The CPU-load argument does not hold either through this path: at 16 kB the block delivers 185 MB/s for 16 percent of a core, which is 0.86 ms of A72 time per megabyte, while the CPU encrypting the data itself spends 0.67 ms per megabyte at 1,490 MB/s. At 64 kB and above the calling core is fully busy. **Unknown (suspected: the `ioctl` copying each block into the kernel and back, plus waiting for the result; the two were not separated).** SHA-256 tells the same story on speed: 165 against 969 MB/s at 16 kB. On CPU time per byte SHA-256 is mixed: at 16 kB the block costs 0.85 ms of A72 time per MB against 1.03 for the CPU, its one cell where it saves CPU, at a sixth of the speed; at 1 kB it costs 9.4 against 1.5.

Untested, so only a guess: the block may win for kernel-side users (IPsec, dm-crypt, the kernel's own TLS) that hand it pages without a copy, for keys the CPU should never see, or when the A72 cores are busy with something else and can wait on an interrupt. None of those is on this image.

**Ask.** If you have the SA2UL running under IPsec or dm-crypt on a J721E, what throughput and CPU load did you see, and on which SDK?
