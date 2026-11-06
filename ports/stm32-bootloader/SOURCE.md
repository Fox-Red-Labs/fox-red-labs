# Source of the stm32-bootloader port for the LP-MSPM0G3519

Terms: [vocab](../../docs/vocab.md).

Upstream: https://github.com/ferenc-nemeth/stm32-bootloader, commit `8848449` (MIT license, `LICENSE`, Ferenc Nemeth). A UART bootloader for the STM32F100 (STM32VLDISCOVERY): Xmodem-CRC with 128-byte and 1024-byte packets, CRC-16, writes the received image to flash from 0x08008000 and jumps to it.

`common/xmodem.c` and `common/xmodem.h` are upstream's `Src/xmodem.c` and `Inc/xmodem.h`, unchanged.

`boards/mspm0g3519/` is the port written by a fresh AI assistant (run 3 of the with-pack condition; the fastest pass, see the experiment page). Its `Src/xmodem.c` and `Inc/xmodem.h` are the same two upstream files. `boards/mspm0g3519-reference/` is a hand-written port used only to prove the host check before any AI assistant ran.

Application area on the G3519: 0x00010000 to 0x0007FFFF; the bootloader sits at 0x00000000 and must fit below 0x00010000.
