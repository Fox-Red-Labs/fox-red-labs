# UART receive with DMA and idle-line interrupt: STM32 project on the NUCLEO-U083RC and six TI boards

MaJerle's `stm32-usart-uart-dma-rx-tx` (MIT, 1,838 GitHub stars on 2026-10-19) is the reference many STM32 users copy for UART receive into a DMA buffer, with the idle-line interrupt marking the end of a burst, two ring buffers and DMA transmit. This folder holds its `usart_rx_idle_line_irq_ringbuff_tx` example moved to the NUCLEO-U083RC (the reference run) and to six TI boards by fresh AI assistants, and the same host check run against every board.

Terms: [vocab](../../docs/vocab.md). Source and commit: [SOURCE.md](SOURCE.md). Write-up: [the experiment page](../../docs/experiments/2026-10-19-stm32-usart-dma-port-matrix.md). Measured with TI Arm Clang 5.1.1.LTS, MSPM0 SDK 2.11.00.07, MSPM33 SDK 1.04.00.00, AM13E2x SDK 26.01.00.03, SysConfig 1.28.0, STM32CubeCLT 1.22.0 (GCC 14.3), STM32CubeU0 1.3.0.

## What the program does

After reset it prints two lines (`USART DMA example: DMA HT & TC + USART IDLE LINE interrupts`, `Start sending data to STM32`). It then parses packets START 0x55, CMD, LEN, LEN data bytes, STOP 0xAA and answers each valid packet with `55 FF 00 AA`. A packet with a wrong stop byte gets no answer and the parser resynchronises on the next 0x55. Received bytes go through a 64-byte DMA buffer, a 128-byte ring, and the parser; replies go through a 128-byte ring and DMA transmit.

## Layout

| Path | What it is |
|---|---|
| `common/lwrb/` | upstream's ring buffer library, unchanged |
| `boards/nucleo-u083rc/` | reference: upstream's G0 example with names and numbers changed; `build.sh` |
| `boards/mspm0g3519/`, `mspm0g5187/`, `mspm0l1306/`, `mspm0c1104/`, `mspm33c321a/`, `am13e230/` | the six TI ports, each as written by one fresh AI assistant; each has its own build file and notes |
| `boards/mspm0g3519-skill-variant/` | the G3519 port by an assistant told to follow TI's migration skill |
| `tools/check.py` | host check, same for every board: banner, packets, wrong stop byte, 200 split packets, 500 back-to-back packets, 100 isolated packets |

Build commands are in each board folder (`ticlang/makefile`, `build.sh`, `Makefile`). The baud rate is one build-time define (`USART_BAUDRATE`, `BAUD`, `UART_BAUD` or `USART_BAUD`, named per board). The check: `python3 tools/check.py <serial port> --baud 115200 --reset "<command that restarts the board>"`.

## Result

At 115200 baud six of the seven boards pass all six checks, three runs each; the AM13E230 port passes five and leaves 12 of 100 isolated packets unanswered until the next one arrives. No TI UART tested offers a usable idle-line event; the ports use a 1 ms timer poll or the UART receive timeout instead. At 921600 baud the C1104 and MSPM33C321A ports (and the G3519 port written under TI's migration skill) lose packets. The table, the differences and the failures are in the experiment page.
