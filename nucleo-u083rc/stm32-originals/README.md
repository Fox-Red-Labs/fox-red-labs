# ST's own STM32U0 examples on the NUCLEO-U083RC

**Short answer: the four that need no button press do.** ST's AES example passes all its own checks (LD4 steady on; its printed output was not compared with `Expected_Results.md`), the ADC example reads sensible values, the PWM example runs at its stated 100 Hz and 50 percent, and the blink toggles. The four that wait for the blue USER button (B1) were not run (checked 2026-10-08).

Built from STM32CubeU0 v1.3.0 (`Projects/NUCLEO-U083RC`, commit c87491e) with ST's GCC 14.3.1 (STM32CubeCLT 1.22.0) at `-Os`, by `../prebuild.sh` (plain compiler calls from each STM32CubeIDE project's file list; no IDE). Flashed with `STM32_Programmer_CLI -c port=SWD -w app.elf -v -rst`. Results read over the debugger with OpenOCD, the core halted for each read.

Terms: [docs/vocab.md](../../docs/vocab.md). HAL is ST's driver library; LL is ST's lower-level driver layer.

| Example | Needs | What was checked | Result |
|---|---|---|---|
| `Examples/CRYP/CRYP_AESModes` | nothing | LD4 (PA5) steady on means every encrypt and decrypt matched; it blinks once a second on any failure (`Error_Handler`) | Pass: LD4 read on 20 times over 3.4 s |
| `Examples/ADC/ADC_MultiChannelSingleConversion` | nothing (one pin, PA0 per the example, left floating) | `uhADCxConvertedData` and the converted values, read three times 1.5 s apart | Analog supply 3300 mV, internal reference 1214 mV (ST's typical 1.212 V), temperature 22 C, floating pin 0.89 to 0.98 V |
| `Examples_LL/TIM/TIM_PWMOutput_Init` | nothing for the first duty cycle | TIM3 registers | PSC 4799 and ARR 99 (100 Hz from 48 MHz), CCR1 50 (50 percent), counter running |
| `Examples/GPIO/GPIO_IOToggle` | nothing | PA5 sampled with the core halted for each read | Toggles. The 100 ms period cannot be read this way: each halt pauses the program's millisecond tick |
| `Examples/RNG/RNG_Config` | B1 press | `aRandom32bit[]` after a press | Not run: needs a press |
| `Examples_LL/RNG/RNG_GenerateRandomNumbers` | B1 press | 8 numbers after a press, LD4 on | Not run: needs a press |
| `Examples/GPIO/GPIO_EXTI` | B1 press | LD4 toggles per press | Not run: needs a press |
| `Examples_LL/USART/USART_Communication_Tx_VCP_Init` | B1 press | text on the Mac's serial port per press | Not run: needs a press |

Notes:

- `CRYP_AESModes` README lists 128, 192 and 256-bit keys; its code sets only `CRYP_KEYSIZE_128B` and `CRYP_KEYSIZE_256B`, which matches the chip (the data sheet says "AES: 128/256-bit key"). Its byte-for-byte output was not compared with `Expected_Results.md`: the example prints only with `USE_VCP_CONNECTION` set to 1 in `main.h` (default 0) or through semihosting in STM32CubeIDE.
- The same examples were ported to the LP-MSPM0G3507. The NUCLEO-G071RB has not been run (page).
