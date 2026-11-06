<!-- experiment
id: E127
board: NUCLEO-U083RC
date: 2026-10-08
status: works
reached: verified
line: ST's CRYP_AESModes example ran as shipped on the NUCLEO-U083RC and passed its own checks (LD4 steady on); its README promises 192-bit keys that the code and the chip do not have. This repo's own timing put the AES block at 65 to 74 times tiny-AES-c by register writes and 20 to 37 times through ST's HAL.
stop: none
-->
# Does ST's AES example run as shipped on the ST NUCLEO-U083RC (STM32U083), and how much faster is the hardware AES than software?

**Short answer.** Yes. ST's `CRYP_AESModes` example, built from the command line with ST's GCC and flashed with ST's programmer, passed every encrypt and decrypt check it makes. On 1 kB of data, this repo's own test put the AES block at 65 to 74 times faster than the tiny-AES-c software library when the code writes the AES registers directly, and 20 to 37 times faster through ST's HAL calls (GCC 12.2 at -O3, 48 MHz). One mismatch: the example's README lists 192-bit keys; the code uses only 128 and 256, which is what the chip supports.

Terms: [vocab](../vocab.md). Board page and fixes: [NUCLEO-U083RC README](../../nucleo-u083rc/README.md) and [FAQ](../../nucleo-u083rc/FAQ.md). 1 kB = 1,024 bytes. HAL is ST's driver library.

**Question.** Does ST's own AES example work out of the package, and how much of the hardware's speed survives ST's driver calls?

**Vendor position.** ST's data sheet DS14463 Rev 2 lists "AES: 128/256-bit key" for the STM32U083 (read 2026-10-07). The example's README says: "In this example, the following key sizes are used: 128, 192 or 256", and "When all ciphering and deciphering operations are successful, LED4 is turned on. In case of ciphering or deciphering issue, LED4 is blinking slowly" (STM32CubeU0 v1.3.0, `Projects/NUCLEO-U083RC/Examples/CRYP/CRYP_AESModes/README.md`).

**What was tried.** NUCLEO-U083RC (STM32U083 Rev A), STM32CubeU0 v1.3.0 at commit c87491e, ST's GCC 14.3.1 and STM32_Programmer_CLI 2.23.0 from STM32CubeCLT 1.22.0, on an Intel Mac. The example was built unchanged from its STM32CubeIDE file list by `nucleo-u083rc/prebuild.sh` and flashed with `STM32_Programmer_CLI -c port=SWD -w app.elf -v -rst`; LD4 (PA5) was read 20 times over 3.4 s through the debugger. For the speed comparison, this repo's own timing image times tiny-AES-c (commit 2385675, unmodified) against the AES block by direct register writes and by HAL `HAL_CRYP_Encrypt`/`Decrypt` in polling mode, on the same 1 kB of data as the LP-MSPM0G3507 run, with FIPS-197 and SP 800-38A vectors checked first. Three runs each; every run gave identical cycle counts.

**How far it got.** Verified. LD4 stayed on through all 20 reads (the failure path toggles it once a second). This repo's timing image passed every vector and matched software output byte for byte over 1 kB.

| AES-128, 1 kB, GCC 12.2 -O3, 48 MHz | Software (tiny-AES-c) | HAL | Registers | Speed-up, HAL | Speed-up, registers |
|---|--:|--:|--:|--:|--:|
| ECB encrypt (cycles) | 397k | 20k | 6.1k | 19.6 | 65.1 |
| ECB decrypt | 724k | 20k | 10k | 35.6 | 71.5 |
| CBC encrypt | 405k | 20k | 6.1k | 20.0 | 66.3 |
| CBC decrypt | 755k | 20k | 10k | 37.1 | 74.4 |
| AES-256 ECB encrypt | 555k | 22k | 8k | 25.4 | 69.7 |

At -O2 the software is slower (527k cycles for ECB encrypt), so the ratios rise to 84 to 91 times by registers. The register path costs about 95 cycles per 16-byte block for AES-128 and 124 for AES-256, which fits the extra rounds AES-256 does. The HAL path costs about 20k cycles per kilobyte either way. The random-number generator filled 1 kB in 26k cycles (0.55 ms), with 50.2 percent ones, no repeated 32-bit word and no error flag.

**The line.** No limit reached. Two things to know: the HAL costs about three times the register path to encrypt 1 kB and about twice to decrypt, and the 192-bit key size in the README does not exist on this chip. The example prints its results only with `USE_VCP_CONNECTION` set to 1 in `main.h` (default 0) or through semihosting in STM32CubeIDE, so the output was not compared byte for byte with the example's `Expected_Results.md`; LD4 is its pass signal.

**Ask.** Has anyone timed ST's HAL CRYP calls against direct register access on another STM32U0 or STM32G0 part?
