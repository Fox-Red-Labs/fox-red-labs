<!-- experiment
id: E4
board: LP-EM-CC2340R5
date: 2026-10-05
status: waiting
reached: run
line: Best 88.74% of 200 kB, which is 177.5 kB in 92 minutes at 33 bytes a second, with a Zigbee plug beside the board. It stopped when the board's block requests stopped reaching Home Assistant for 30 s. Twelve transfers, none finished.
stop: radio-link
-->
# Can Home Assistant update a TI LP-EM-CC2340R5 Zigbee device over the air?

**Short answer.** No, but close. With a mains-powered Zigbee plug beside the board, one transfer reached 88.74% (177.5 kB of 200.1 kB) in 92 minutes at 33 bytes a second, then stopped when the board's requests stopped arriving for 30 s. Twelve transfers over three tries (2026-10-05 and 2026-10-07), none finished. The radio link was the main limit: from the desk with no plug nearby, the best was 7.20%.

Terms: [vocab](../vocab.md) (OTA, ZHA, MCUboot, end device, router, LQI, RSSI). Errors and fixes: [CC2340R5 FAQ](../../cc2340r5/FAQ.md). 1 kB = 1,024 bytes. Contents: first try (below), [second try, 2026-10-07](#second-try-2026-10-07), [third try, with a plug beside the board](#third-try-2026-10-07-with-a-zigbee-plug-beside-the-board), then the line, ideas and the ask at the end.

**Question.** Home Assistant's built-in Zigbee integration (ZHA) can update many Zigbee devices by radio. Can it update a TI CC2340R5 running TI's own OTA example?

**Vendor position.** TI's `onoff_switch_ota_client_onchip` example is written for TI's own OTA server. It comes from F3 SDK 9.21.00.36. Its README says to package the image with `zOTAfileGen` from the CC13xx/CC26xx SDK.

**What was tried.** First try, 2026-10-05. Home Assistant 2026.9.2 with zigpy 2.2.0, a Nabu Casa ZBT-2 Zigbee stick in a Mac, TI's OTA client example with TI's MCUboot bootloader. Four fixes were needed before an update was even offered:

1. `zOTAfileGen` is not in the F3 SDK. **Tool or docs.** A 15-line script writes the standard Zigbee OTA file instead (`cc2340r5/matrix/make_zigbee_ota.py`); zigpy's own parser accepted it.
2. The example's endpoint description leaves out the OTA cluster its code uses. **Tool or docs.** Home Assistant never offered an update, and zigpy logged `Ignoring message on unknown cluster: 0x0019`. Fix: `cc2340r5/matrix/ota_endpoint.patch.c`.
3. The board was re-paired before Home Assistant had finished removing it, which left a stale record. Wait until the device is gone from the list before re-pairing.
4. zigpy caches the update folder for 24 hours. **Third-party behavior.** A new file is not seen until ZHA is reloaded.

**How far it got.** Four transfers of the 200.1 kB image (200 kB) started; none finished.

| Transfer | Board | Got to | How it ended |
|---|---|---|---|
| 1 | TI's example with this repo's fix: a sleeping end device | 1.03% in 42 s | A block reply was not delivered; the board stopped asking. |
| 2 | Same | 0.32% in 63 s | Gaps of up to 17 s between requests; zigpy aborted. |
| 3 | Receiver forced on in C | 0.71% in 9 s, about 150 bytes a second | `ZIGBEE_DELIVERY_FAILED: 3074`, then abort. |
| 4 | Same, board moved closer | 1.34% in about 2 minutes | Stopped; cause not logged. |

As TI's example ships, the board is a sleeping end device that checks for messages every 3 seconds (`ED_POLL_RATE 3000`). **TI example default.** The cause of the longer gaps in transfer 2 was not found.

## Second try, 2026-10-07

**Expectation, written before the run.** The plan was to build TI's client as a router instead of an end device. A router keeps its receiver on and has no parent and no polling. That removes all three ways the first try lost block replies. Expected: past 1.34%, about 70% likely; finished within 30 minutes, about 35%.

**The router build does not fit.** **Unknown (suspected: Chip limit).** It links, with 274.7 kB of flash used and 5.3 kB of RAM left. TI's signing step then stops with `Error: Image size (0x32707) + trailer (0x630) exceeds requested size 0x32000`. The compressed update image is 201.2 kB, and the update slot is 200 kB. The router also reserves 8 kB of storage for its network settings at the top of flash, where an end device reserves 4 kB. That overlaps the update slot by 4 kB. SysConfig only warns: `Region Base + Region Size overlaps with NVS region: CONFIG_NVSINTERNAL_ZB`. Moving the slots does not help on this 512 kB chip. Cut the bootloader area to 24 kB, against its 22.4 kB need, and cut the running slot to fit the router image. The update slot is still about 1.3 kB short. Only TI's layout and compression settings were tried. Diff: `ota_zr_v1.diff` in `cc2340r5/matrix/`.

**What ran instead: the first try's setup again, as a control.** TI's example as shipped, which is a sleeping end device that polls every 3 s. This repo's OTA-cluster fix and all-channel search were kept. The update image was built with TI's `OTA_VER=2.0.0+0` so its bootloader version is higher than the running one. Home Assistant 2026.9.2, zigpy 2.2.0, the ZBT-2 on the Mac, and the board on the same desk. The update file was copied into the same folder, and ZHA was reloaded once to clear its cache; Home Assistant was not restarted, and `configuration.yaml` was not changed.

| Transfer | Got to | Time | How it ended |
|---|---|---|---|
| 5 | 150 bytes (0.07%) | 1 s | ZHA unloaded and reloaded itself mid-transfer. It did again in transfer 10, with nothing else using Home Assistant. **Unknown (suspected: Third-party behavior).** |
| 6, 7 | No block requested | | `OTA image_notify handler exception` (a send timeout in the coordinator driver). The board was not heard again, even after a reset. Reflashing it brought it back. **Unknown.** |
| 8 | **14.4 kB (7.20%)** | 11 min 56 s, 20.6 bytes a second | 3 lost block replies were recovered; the 4th ended it: `Failed to deliver message: <sl_Status.ZIGBEE_DELIVERY_FAILED: 3074>`, then `<Status.TIMEOUT: 148>`. |
| 9 | 4 kB (2.00%) | 5 min 2 s, 13.6 bytes a second | Started again from byte 0; 6 lost replies recovered, then two close together ended it. |

During transfers 8 and 9, Home Assistant read the board at LQI 36 to 84, median 52, and RSSI -91 to -79 dBm, median -87. That is 226 readings 5 s apart. On 2026-10-05 the readings were LQI 36 to 48. In transfer 8 the coordinator dropped 90 repeated requests from the board, a sign that its acknowledgements did not reach it. Also in transfer 8, 17 gaps between blocks were longer than 10 s, the longest 27.9 s.

**Why two lost replies end a transfer.** The two sides time out differently. Home Assistant's Zigbee library, zigpy, aborts after 30 s with no new block request: `MAX_TIME_WITHOUT_PROGRESS = 30` in `zigpy/ota/manager.py`. TI's Zigbee stack (ZBOSS) asks again for a missing block after 15 s, up to three times. That is `ZB_ZCL_OTA_UPGRADE_RESEND_BUFFER_DELAY` in the SDK's `zboss_r23/include/zcl/zb_zcl_ota_upgrade.h`, compiled into TI's library. One lost reply costs 15 s. Two in a row reach 30 s and zigpy stops. zigpy also sends at most 50 bytes a block, whatever the device asks for. The client asks for 64. One listed manufacturer is the exception.

**Two more things found.** This bootloader build refuses an update image that is not newer than the running one (`MCUBOOT_DOWNGRADE_PREVENTION`), so an update built with the same version, 1.0.0, does not install even at 100%. TI's example README says to build the update with `OTA_VER`. **Third-party behavior:** the board rejoined with the same address after being reflashed as a different kind of device. ZHA kept the old device's description and offered no update. Removing the device in ZHA, then resetting the board, fixed it.

**The board afterwards.** Restored from a full backup taken before the test and read back. 255 of 256 flash pages, 2 kB each, were identical, and so was the boot configuration. One page in the network-settings area differed by 256 bytes, written by the restored firmware after it started. The test device was removed from ZHA.

## Third try, 2026-10-07, with a Zigbee plug beside the board

**What changed:** one thing, the radio path. A Third Reality smart plug, a mains-powered Zigbee router, was moved next to the desk. The board, firmware, update file and Home Assistant were the same as in the second try. The board joined through that plug. The plug's neighbor table listed it as a child at LQI 111, two hops from the coordinator.

| Transfer | Got to | Time | How it ended |
|---|---|---|---|
| 10 | 1.9 kB (0.95%) | 20 s, about 100 bytes a second | ZHA unloaded and reloaded itself mid-transfer, as in transfer 5. **Unknown (suspected: Third-party behavior).** |
| 11 | **177.5 kB (88.74%)** | 91 min 49 s, 33.0 bytes a second | 4 lost block replies, all recovered by the board's 15 s re-request. Then no new request reached Home Assistant for 30 s, with no failed send logged. zigpy ended it: `<Status.TIMEOUT: 148>`. |
| 11, retried | 78.5 kB (39.25%) | 44 min 12 s, 30.3 bytes a second | The board started again from byte 0 instead of 88.74%. One undelivered reply (`OTA image_block handler exception`), then `<Status.TIMEOUT: 148>`. |

During transfer 11 Home Assistant read the last hop at LQI 16 to 104, median 92, and RSSI -96 to -74 dBm, median -77. That is 1,099 readings 5 s apart. The second try had a median of LQI 52 and -87 dBm. The rate was 33 bytes a second against 20.6. There were still 228 repeated requests dropped as duplicates. There were also 52 gaps over 10 s between blocks, the longest 24.1 s. At that rate a whole image takes about 103 minutes, so every minute is another chance for the 30-second limit to end it.

**ZHA reloaded itself twice during transfers.** **Unknown (suspected: Third-party behavior).** Both times, Home Assistant's log shows the ZHA gateway gone: `ValueError: No gateway object exists` on this repo's status reads. It also shows `Unloading socket://127.0.0.1:6638 (zha) config entry` once at each reload, and the update ends with `<Status.TIMEOUT: 148>`. Nothing reloaded it on purpose. The small program that passes the ZBT-2's serial port to Home Assistant on the Mac logged Home Assistant's client disconnecting. It logged no serial error.

**The board afterwards.** Restored from a backup taken before this try and read back. 255 of 256 flash pages were identical, and so was the boot configuration. One network-settings page differed by 256 bytes. The test device was removed from ZHA. The new image never ran, so its version was never read back.

**The line.** **Unknown (suspected: Radio environment, then Third-party behavior).** Best 88.74% of 200.1 kB. The radio link set how far each try got. With no plug near the board, the LQI median was 52 and the best was 7.20%. With a plug beside it, the LQI median was 92 and the best was 88.74%, on the same firmware. What ends a transfer is zigpy's 30-second limit with no new block request, against a board that re-asks only after 15 s and sends one 50-byte block request at a time. A sleepy end device at 33 bytes a second needs about 103 minutes, so one bad half-minute anywhere ends it, and TI's client then starts over from byte 0. Readings on 2026-10-05, none taken during a transfer, were LQI 36 to 48 and RSSI -88 to -91 dBm; they were not saved.

- The receiver-on end device (transfers 3 and 4 of the first try) with the plug beside the board: it moved about 150 bytes a second when the link held, which would finish in about 23 minutes.
- A local change to zigpy's 30-second limit, to see whether the transfer then finishes (a test only; it changes Home Assistant's code).
- Send the unpadded image, 158.2 kB (21% less) instead of 200 kB. TI's example README says to send the padded one because only it carries the marker the bootloader looks for (the unpadded one needs `mark_fw_ready()` enabled in the client); its `.syscfg` comments still say the opposite.
- A shorter poll interval during an update. Larger blocks are not a setting: zigpy caps them at 50 bytes.
- Make the router image about 9 kB smaller, or change the compression settings, so its update fits the slot (not run).
- A board right beside the coordinator, with no router between, for one transfer.

**Ask.** Has anyone completed a ZHA update of a TI F3 device? What block size and poll settings did it take? Has anyone run a slow, sleepy device through zigpy's 30-second stall limit, and how?
