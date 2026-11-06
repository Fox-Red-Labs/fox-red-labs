# cc2340r5: switch the board's LED from Home Assistant over Zigbee

**What it is.** TI's `onoff_light` example on an LP-EM-CC2340R5, joined to Home Assistant through a ZBT-2 Zigbee coordinator (ZHA). Home Assistant switches the board's red LED. Build steps: [docs/builds/zigbee-light.md](https://github.com/Fox-Red-Labs/fox-red-labs/blob/main/docs/builds/zigbee-light.md). The build page status is Could work: it ran once and a repeat did not join.

**What it runs on.** `cc2340r5` (LP-EM-CC2340R5), SimpleLink Low Power F3 SDK 9.21.00.36 LTS, TI Arm Clang 5.1.1, SysConfig 1.26.3, UniFlash 9.6.0, on an Intel Mac with macOS 15.7.9.

**Shown by.** On 2026-10-05 the board joined 37 seconds after the flash finished, and 20 of 20 on/off commands from Home Assistant landed, median 95 ms until Home Assistant showed the new state. One run on one desk; after a re-pairing two hours later the same image answered in about 5 s. Log and method: [cc2340r5/zigbee/README.md](https://github.com/Fox-Red-Labs/fox-red-labs/blob/main/cc2340r5/zigbee/README.md).

**What did not work.** TI's example searches channel 11 only and forms its own network, so a board never finds a home network on another channel. Two lines in the `.syscfg` fix that: router role, all 16 channels. A repeat on 2026-10-16 with the same image did not join in three tries of about four minutes each; the cause is open. If you repeat this build, your result, joined or not, is the most useful reply to this post.

Built on the lab's bench, not by a reader.
