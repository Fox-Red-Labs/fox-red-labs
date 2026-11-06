# stm32-bootloader on the LP-MSPM0G3519 (UART Xmodem bootloader)

A UART bootloader (Xmodem-CRC) moved from the STM32F100 to the LP-MSPM0G3519. Source and layout: `SOURCE.md`. The write-up is the porting-pack experiment.

Terms: [vocab](../../docs/vocab.md), [LP-MSPM0G3519 FAQ](../../mspm0g3519/FAQ.md).

Behavior the host check confirms, from the Mac over the board's USB serial port at 115200 baud: the upstream banner and prompt, `C` until the first packet, Xmodem-CRC with 128-byte and 1024-byte packets, a NAK for a wrong CRC and the retry accepted, CAN CAN after the third error then `Failed... Please try again.` and the prompt again, on EOT `Firmware updated!` and `Jumping to user application...`, the received bytes in flash at 0x00010000, and the application running with its timer interrupt. A reset with a stored application jumps to it. One change from upstream: nobody can press a button during the check, so the bootloader stays in update mode when the first word of the application area is 0xFFFFFFFF.

## Build and check

```bash
cd boards/mspm0g3519 && make          # SysConfig 1.28.0 and TI Arm Clang 5.1.1; MSPM0 SDK 2.11.00.07; bootloader.out
python3 tools/ai_runs/portboard.py flash g3519 boards/mspm0g3519/bootloader.out
tools/run_g3519.sh                     # 6 checks over the console; erases the application area through the probe (MG000001)
```

`tools/check.py` is board-independent (it takes the erase, dump and reset commands as arguments); `tools/g3519.py` holds the G3519 ones. `tools/testapp/` builds the two test applications in `tools/images/` (linked at 0x00010000, print `APP OK v<n>` and `APP TICK` every 250 ms from a timer interrupt). `tools/xsend.py` is the Xmodem-CRC sender. `tools/timing.py` measures the update time.

Results: six checks per port, sizes and update time are in the experiment page above.
