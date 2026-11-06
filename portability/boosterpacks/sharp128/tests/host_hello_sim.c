/* Host simulation of the hello example with an SD card: runs hello.c (FatFs, long names, folder listing,
 * file read) against the simulated card, then prints the last panel frame of each screen as text.
 * Build and run: tests/run_sim.sh (second part). */
#include "../sharp_lcd.h"
extern struct { uint32_t magic, full_us, row_us, rows16_us, frame_bytes, spi_hz; int32_t sd_present, sd_init_rc; uint32_t sd_sectors; int32_t sd_mount_rc, sd_files, sd_file_bytes; uint32_t counter, loops, stack_free; } hello_res;
#define SIM_DONE() (hello_res.counter >= 3)
#include "sim_port.h"
extern int hello_main(void);

static uint8_t rev(uint8_t b) { uint8_t r = 0; int i; for (i = 0; i < 8; i++) if (b & (1 << i)) r |= (uint8_t)(0x80 >> i); return r; }

int main(int argc, char **argv)
{
    static uint8_t scr[2048]; uint32_t p, shown = 0;
    if (argc < 2) return 2;
    img = fopen(argv[1], "rb"); if (!img) { perror("img"); return 2; }
    if (!setjmp(done)) hello_main();
    for (p = 0; p < sessions; p++) {
        uint32_t q0 = sess_at[p] + 1, end = (p + 1 < sessions) ? sess_at[p + 1] : lcd_n, n = 0;
        while (q0 + 18 <= end) {
            uint8_t a = rev(lcd[q0]); uint32_t j;
            if (a < 1 || a > 128) break;
            for (j = 0; j < 16; j++) scr[(a - 1) * 16 + j] = rev(lcd[q0 + 1 + j]);
            q0 += 18; n++;
        }
        if (n == 128 || p + 1 == sessions) {      /* print each full-frame update, and the final state */
            if (n == 128) shown++;
            if (n == 128 || p + 1 == sessions) {
                uint32_t x, y;
                printf("--- panel after session %u (%u rows in it)\n", p, n);
                for (y = 0; y < 128; y += 2) { for (x = 0; x < 128; x += 1) putchar(((scr[y * 16 + (x >> 3)] >> (x & 7)) & 1) ? ' ' : '#'); putchar('\n'); }
            }
        }
    }
    printf("hello_res: sd_present %d init %d sectors %u mount %d files %d file_bytes %d full_us(virtual) %u frame_bytes %u counter %u\n",
           hello_res.sd_present, hello_res.sd_init_rc, hello_res.sd_sectors, hello_res.sd_mount_rc, hello_res.sd_files, hello_res.sd_file_bytes, hello_res.full_us, hello_res.frame_bytes, hello_res.counter);
    return !(hello_res.sd_files > 0 && hello_res.sd_file_bytes > 0);
}
