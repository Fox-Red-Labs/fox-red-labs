/* Ours: the smallest u8x8 text and u8g2 font program for an SSD1306 128x64, to see whether u8g2 compiles and
 * links for the C28x (16-bit char). Compiled and linked only; never run. The byte callback stores what would go
 * out on I2C, one byte per array element, so nothing depends on a peripheral. */
#include "u8g2.h"

#define OUT_MAX 2048
volatile uint8_t out_bytes[OUT_MAX];
volatile unsigned out_n;

static uint8_t byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    (void)u8x8;
    if (msg == U8X8_MSG_BYTE_SEND) {
        const uint8_t *p = (const uint8_t *)arg_ptr;
        while (arg_int--) { if (out_n < OUT_MAX) out_bytes[out_n++] = *p; p++; }
    }
    return 1;
}

static uint8_t gpio_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    (void)u8x8; (void)msg; (void)arg_int; (void)arg_ptr;
    return 1;
}

static u8x8_t u8x8;
static u8g2_t u8g2;
static uint8_t tile_buf[128 * 8];   /* full frame: 8 pages of 128 columns, one byte each on the wire */

void text_u8x8(void)
{
    u8x8_Setup(&u8x8, u8x8_d_ssd1306_128x64_noname, u8x8_cad_ssd13xx_fast_i2c, byte_cb, gpio_cb);
    u8x8_InitDisplay(&u8x8);
    u8x8_SetPowerSave(&u8x8, 0);
    u8x8_SetFont(&u8x8, u8x8_font_chroma48medium8_r);
    u8x8_DrawString(&u8x8, 0, 0, "Hello C28x");
}

void text_u8g2(void)
{
    /* The body of u8g2_Setup_ssd1306_128x64_noname_f with our own buffer, so the link does not pull every display. */
    u8g2_SetupDisplay(&u8g2, u8x8_d_ssd1306_128x64_noname, u8x8_cad_ssd13xx_fast_i2c, byte_cb, gpio_cb);
    u8g2_SetupBuffer(&u8g2, tile_buf, 8, u8g2_ll_hvline_vertical_top_lsb, U8G2_R0);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
    u8g2_DrawStr(&u8g2, 0, 10, "Hello C28x");
    u8g2_SendBuffer(&u8g2);
}

#ifdef FRL_HOST
#include <stdio.h>
int main(void)              /* host emulation only (host_emulate.sh): print the bytes, low 8 bits of each */
{
    text_u8x8();
    text_u8g2();
    for (unsigned i = 0; i < out_n; i++) printf("%02x%c", out_bytes[i] & 0xff, (i % 32 == 31) ? '\n' : ' ');
    printf("\n# %u bytes; uint8_t is %u bits\n", out_n, (unsigned)(sizeof(uint8_t) * 8));
    return 0;
}
#else
int main(void)
{
    text_u8x8();
    text_u8g2();
    for (;;) { }
}
#endif
