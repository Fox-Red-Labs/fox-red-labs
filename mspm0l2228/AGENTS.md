# AI assistant notes for the TI LP-MSPM0L2228 LaunchPad development kit

The chip is a 32 MHz Cortex-M0+ with a segment LCD, AES and a battery-backed domain. The tools are the LP-MSPM0G3519's. DSLite (4.2 s) and xPack OpenOCD 0.12.0-7 (20.6 s) both flash it. Each line below comes from a run on one board, 2026-11-09.

Terms: [docs/vocab.md](../docs/vocab.md) and the [LP-MSPM0L2228 FAQ](FAQ.md). **SysConfig** is TI's pin and clock setup tool; **DSLite** is TI's command-line flasher; **NONMAIN** is the flash area that holds the chip's boot settings.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Paths below starting `mspm0l2228/` or `data/` are in the Fox Red Labs repository.

## The board

| Part | Fact |
|---|---|
| Chip | MSPM0L2228: Cortex-M0+, 32 MHz SYSOSC, MCLKCFG FLASHWAIT reads 2; 256 kB flash, 32 kB RAM; DEVICEID 0x1BB9F02F (`data/results/mspm0l2228.csv`) |
| LEDs | This board is Rev. A: silkscreen puts blue on PA16, red PB10, green PB9. TI's guide and gpio_toggle_output say blue is PA23 (Rev E; E2E thread 1497347), so blue may stay dark. The one-command run timed red PB10 to 150,150,150,900 ms; TI's example toggles PA23, PB10, PB9 and PA21 (USER_TEST) and the pin reads only showed outputs changing. After the restore the pin that toggled was PA0 (red LED LED3), not the green RGB pin TI's README names (`data/results/mspm0l2228.csv`, tests `time_to_blink`, `backup_restore`) |
| Serial | `/dev/cu.usbmodemML2200011`, UART0 at 115200; silent for 5 s after plug-in (test `serial_idle`) |
| Probe | XDS110 serial ML220001, firmware 3.0.0.43 after an update (`FAQ.md`) |

## Start here

| Step | This board |
|---|---|
| Tools | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0, DSLite from CCStudio 21.0.1 (`mspm0l2228/README.md`) |
| Connect | Check `xdsdfu -e` first; DSLite refuses firmware 3.0.0.28 (`FAQ.md`) |
| Build TI's blink | `mspm0g3507/mcu-bench/sdk-common/sdkmake.sh gpio_toggle_output/ticlang`, 2.5 to 3.6 s (test `blink_build`) |
| Flash | `mspm0l2228/bench/dsflash.sh <image.out>` (DSLite `flash -e -f -v`), 4.2 to 4.8 s (test `blink_flash`) |
| Read a result | `mspm0l2228/bench/dsread.sh <address> <words>`; reads do not reset the chip and each takes about 2 s |
| Probe lock | Run every probe command under `python3 tools/frl/lock.py exec --serial ML220001 --` |

## Rules this board taught

- Check a first blink with the output registers: GPIOA DOUT is 0x400A1280 and GPIOB DOUT is 0x400A3280. A read per 2 s shows that bits change, not the period (test `time_to_blink`).
- Build CoreMark with `mspm0l2228/bench/coremark/build.sh` and run it with `run.sh <opt> <wait> <run>`. A run of 1.5k iterations needs a 30 to 50 s wait before the read (test `coremark`).
- Expect 74.58 per second at -O3, not the 78.84 the LP-MSPM0G3519 gives at 32 MHz. Runs repeat exactly (test `coremark`).
- The timing test uses PB9 (the green LED) for the GPIO store, and interrupt 28 (AESADV) as the spare line (test `irq_latency`).
- Back up main flash and NONMAIN first. Restore with `DSLite flash -c <ccxml> -e -f -v mspm0l2228/data/shipped-main-flash.bin,0x0` (17 s) and read both regions back; NONMAIN stayed identical (test `backup_restore`).
- Never write NONMAIN (0x41C00000), and run no `bsl/`, `boot_manager/` or `flashctl_nonmain_memory_write` example. The sweep here only built examples (test `examples_built`).
- `./start.sh mspm0l2228 blink` is tested: backup, build 3.0 s, OpenOCD write and read-back 20.6 s, red LED PB10 timed, shipped image restored and read back identical, 41 s in all (test `time_to_blink`).
- TI's LCD example wrote LCD memory (LCDCTL0 0x181D); nobody looked at the glass (test `port_example_behavior`).

<!-- recovery:start -->
## When the board stops answering

Checked 2026-11-09. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- Not seen yet. Expected from the family: TI's low-power examples (sysctl_power_policy_sleep_to_*, *_standby, *_stop, sysctl_shutdown) cut the debug link (`mspm0g3507/AGENTS.md`, `docs/arrivals/mspm0l2228.md`)

Restore: frl restore <backup folder> --probe ML220001, or DSLite flash -c mspm0l2228/MSPM0L2228_ML220001.ccxml -e -f -v mspm0l2228/data/shipped-main-flash.bin,0x0; read back and compare. Regions: main flash (256 kB); NONMAIN read only, never written. (`data/results/mspm0l2228.csv`)

<!-- recovery:end -->
