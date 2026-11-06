# Source of the stm32-usart-dma-rx-tx port (NUCLEO-U083RC and TI boards)

Terms: [vocab](../../docs/vocab.md).

Upstream: https://github.com/MaJerle/stm32-usart-uart-dma-rx-tx, commit `c242589d766eada3ad1d795630f2b1c2482a2af8` (2026-09-12), MIT license (`LICENSE`, Tilen Majerle). 1,838 stars on 2026-10-19.

Ported example: `projects/usart_rx_idle_line_irq_ringbuff_tx_G0` (UART receive with DMA and idle-line interrupt, two lwrb ring buffers, DMA transmit, a 5-byte packet parser).

`common/lwrb/` is `middlewares/lwrb` from that commit, unchanged (MIT, same license file).

`boards/nucleo-u083rc/` is the example moved from the NUCLEO-G071RB to the NUCLEO-U083RC by changing names and numbers only (header names `g0xx` to `u0xx`, SYSCFG on the APB1 group 2 clock, pin alternate function 7, interrupt name `USART2_LPUART2`, PLL 6/2 for 48 MHz, flash latency 1, baud as a build-time define). Everything else is upstream's `main.c`.
