/*
 * Full-frame time of the EDUMKII screen through the portable core: one 128 x 128 x 16-bit frame
 * (32,768 bytes) sent as a single block (edumkii_blit_raw) and as a full-screen fill (256-byte chunks),
 * polled and by DMA, at 16 MHz and 8 MHz SPI. Best of 5, cycle counter. Results stay in RAM (struct fb)
 * for the debugger. Boards with SPI DMA only (EDU_SPI_DMA). BSD-3-Clause.
 */
#include "ti_msp_dl_config.h"
#include "board_config.h"
#include "edumkii.h"
#include "edu_board.h"

#ifndef EDU_SPI_DMA
#error "framebench needs a board with SPI DMA"
#endif

#define NRUN 4
volatile struct {
    uint32_t magic;                     /* 0xFB000001 when done */
    uint32_t cpu_hz;
    struct { uint32_t scr, dma, blit_cyc, fill_cyc; } r[NRUN];
} fb;

static uint8_t frame[EDUMKII_LCD_W * EDUMKII_LCD_H * 2] __attribute__((aligned(4)));

int main(void)
{
    static edumkii_t d;
    SYSCFG_DL_init();
    edu_board_init();
    fb.cpu_hz = edu_cpu_hz();
    edumkii_init(&d, edu_port());
    edumkii_lcd_init(&d);
    edumkii_backlight(&d, 1);
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 128; x++) {
            uint16_t c = EDUMKII_RGB(x * 2, y * 2, (x ^ y) * 2);
            frame[(y * 128 + x) * 2] = (uint8_t)(c >> 8);
            frame[(y * 128 + x) * 2 + 1] = (uint8_t)c;
        }
    static const uint32_t scr_list[2] = { 0, 1 };           /* 16 MHz, 8 MHz at a 32 MHz clock */
    int k = 0;
    for (int s = 0; s < 2; s++)
        for (int m = 0; m < 2; m++) {                       /* 0 = polled, 1 = DMA */
            edu_spi_rate(scr_list[s]);
            edu_use_dma(m);
            uint32_t bb = ~0u, bf = ~0u;
            for (int i = 0; i < 5; i++) {
                uint32_t t0 = edu_stamp();
                edumkii_blit_raw(&d, 0, 0, 128, 128, frame);
                uint32_t c = edu_cycles_since(t0); if (c < bb) bb = c;
                t0 = edu_stamp();
                edumkii_fill(&d, (i & 1) ? EDUMKII_RED : EDUMKII_BLUE);
                c = edu_cycles_since(t0); if (c < bf) bf = c;
            }
            fb.r[k].scr = scr_list[s]; fb.r[k].dma = (uint32_t)m; fb.r[k].blit_cyc = bb; fb.r[k].fill_cyc = bf;
            k++;
        }
    edu_use_dma(1); edu_spi_rate(0);
    edumkii_blit_raw(&d, 0, 0, 128, 128, frame);
    fb.magic = 0xFB000001u;
    for (;;) { }
}
