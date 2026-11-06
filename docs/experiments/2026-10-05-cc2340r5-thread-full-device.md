<!-- experiment
id: E1
board: LP-EM-CC2340R5
date: 2026-10-05
status: line found
reached: compile
line: Fails to link with TI's configurations and this repo's toolchain: each Thread node build was 7.5 kB to 23.8 kB short of RAM, depending on build options. All link on the 64 kB CC2340R53; not run.
stop: ram
-->
# Can the TI LP-EM-CC2340R5 run Thread as an end device or router on its own?

**Short answer.** Not with TI's configurations built with the settings used here: every Thread node build compiled and then needed 7.5 kB to 23.8 kB more RAM than this chip has, depending on build options. The same builds linked for the 64 kB CC2340R53. A trimmed build might fit; not run.

Terms: [vocab](../vocab.md) (RCP, MTD, FTD, NCP, NVS, .bss). Errors and fixes: [CC2340R5 FAQ](../../cc2340r5/FAQ.md). 1 kB = 1,024 bytes.

**Question.** TI lists the CC2340R52 chip on this board as a Thread radio co-processor (RCP) only: the radio, driven by a bigger computer. Can it be a full Thread device on its own?

**Vendor position.** TI's OpenThread release notes list the CC2340R5 as RCP only, evaluation. They list the CC2340R53, with 64 kB RAM, as RCP, MTD, FTD and NCP. TI does not say why. Source: `TexasInstruments/ot-ti`, branch `thread-v1.4-ti`, `docs/ti-openthread-release-notes.md`, read 2026-10-05.

**What was tried.** `ot-ti`, branch `thread-v1.4-ti`, on an Intel Mac with macOS 15.7.9, 2026-10-05. This repo's setup differs from TI's. GCC 12.2 from the Zephyr SDK was used instead of TI's tested toolchain. SysConfig 1.26.3 was used, where ot-ti expects 1.27.0. The F3 SDK version ot-ti pins, 9.20.00.86, was used, built by hand because TI's bootstrap script is Linux-only. Two board-name checks were removed that skip the node builds for this board without a message: `examples/apps/cli/CMakeLists.txt` lines 49 to 56 and `examples/apps/CMakeLists.txt` line 38. The settings storage (NVS) was also moved out of the code area. Diff: `cc2340r5/thread/ot-ti-local.patch`. Build commands: `cc2340r5/thread/README.md`.

**How far it got.** Every build configured and compiled, and fits in flash. The largest is 417.5 kB of about 496 kB. The table shows the link step for the default build of each. The table quotes the exact errors. Other build options overflowed by up to 23.8 kB.

| Build | RAM needed | On the CC2340R52 (36 kB) | On the CC2340R53 (64 kB) |
|---|--:|---|---|
| RCP (the supported one) | 27.2 kB | links, 8.8 kB spare | links. |
| CLI MTD (end device) | 46 kB | `region 'SRAM' overflowed by 10216 bytes` | links. |
| NCP MTD | 43.5 kB | `region 'SRAM' overflowed by 7632 bytes` | links. |
| CLI FTD (router) | 52 kB | `region 'SRAM' overflowed by 16400 bytes` | links. |
| NCP FTD | 56.7 kB | `region 'SRAM' overflowed by 21176 bytes` | links. |

**The line.** The link step, on RAM. **Unknown (suspected: Chip limit).** The smallest full Thread node built needs 7.5 kB more than the chip has. Only TI's configurations were tried, with this repo's toolchain and with TI's board checks removed. The numbers line up with TI's listing; TI does not give its reason, and a trimmed build might fit. **Tool or docs:** the build skips these apps with a board-name check and no message.

Also found: as shipped, even the RCP (the one TI supports here) did not link. The CC2340R53 build gave the same error, and the CC2340R52 build gave it on 2026-10-05: `section .nvs VMA [0000000000010000,0000000000013fff] overlaps section .text`. The settings storage is placed inside the code area for the CC2340R52 and R53 builds that were linked. **Unknown (suspected: Tool or docs).** This repo's different toolchain and SysConfig version could contribute; TI's exact setup was not tested.

The RCP linked after that fix but was never flashed or run in this experiment. On 2026-10-06 an ot-rcp image with the CCFG checksums filled in was flashed and, driven by OpenThread on a Mac, joined a Thread network led by the LP-CC1354P10-1 as a router: `board-to-board/thread-two-radios-2026-10-06/README.md`.

- Trim the NCP MTD by 7.5 kB. For a sense of where RAM goes, the CLI MTD's largest users (the NCP MTD was not broken down): the OpenThread instance 12 kB, the application stack 8 kB, the command-line interpreter 7 kB, the FreeRTOS heap 3.3 kB. A smaller stack and no command line might close the gap.
- Smaller OpenThread build options (fewer neighbor and child table entries, no diagnostics).
- Use the RCP as TI intends, behind a computer running OpenThread's border router.
- Use the LP-EM-CC2340R53 or a CC27xx board.

**Ask.** Has anyone fit an OpenThread end device into 36 kB on a Cortex-M0+? Which options mattered most?
