# usart_rx_idle_line_irq_ringbuff_tx_G0 on the LP-MSPM33C321A

Terms: [vocab](../../../../docs/vocab.md). Port of MaJerle/stm32-usart-uart-dma-rx-tx `projects/usart_rx_idle_line_irq_ringbuff_tx_G0` (commit c242589d, MIT,
`LICENSE-upstream-MIT`). `middlewares/lwrb` is copied unchanged. MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS,
SysConfig 1.28.0. Console: XDS110 back-channel, UC12 on PA10 (TX) / PA11 (RX), 115200 8N1.

    ./build.sh [baud]     # baud = the one build-time define USART_BAUD (default 115200); checks BCR/BSL use is 0
    ./flash.sh            # DSLite through the bench lock, probe <probe serial> only
    ./console.py --reset r2 5501 01FFAA r0.5

## What changed from the original
- STM32 LL/DMAMUX/RCC/SysTick setup replaced by SysConfig (`usart_dma_rx_idle_tx.syscfg`) + DriverLib calls.
  Clock is TI's default 32 MHz (original ran 64 MHz); the UART divisor comes from `USART_BAUD` and the UART clock.
- RX DMA: UC12 RX trigger, repeat-single mode (one byte per trigger, buffer of 64 reloads itself). Position is read
  from the DMA's remaining-count register, as in the original. A reading of 0 is treated as position 0.
- TX DMA: UC12 TX trigger, single mode, source/length set per transfer from `lwrb_get_linear_block_read_*`.
- **No DMA half-transfer interrupt on this chip.** Only "block done" (UART `DMA_DONE_RX`) exists. HT is dropped.
- **No USART IDLE line interrupt.** The UART receive timeout (RTOUT) only fires while bytes are in the RX FIFO.
  With the FIFO set to 3/4 and a timeout-triggered DMA, any burst whose length is a multiple of 3 was drained by DMA,
  RTOUT never fired and the data sat unprocessed (seen on the board: 15 bytes gave no answer until the next bytes
  arrived). With one-entry trigger the DMA empties the FIFO on every byte, so RTOUT cannot fire at all.
  Closest mechanism used: a 1 ms SysTick interrupt (priority 0, same as UC12, so no preemption) calls `usart_rx_check()`.
  Idle is therefore detected up to 1 ms after the last byte, not one character time.
- DMA done RX/TX and the UART share the single UC12 vector (one handler instead of a DMA vector plus a USART vector).
- Unchanged behavior kept on purpose: a wrong stop byte is consumed (even if it is 0x55), no answer is sent.
