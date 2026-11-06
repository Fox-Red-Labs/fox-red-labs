/* Host simulation of the whole player path: the real sharp_lcd.c, sharp_sd.c, FatFs or Petit FatFs and
 * player.c run on the Mac against a mock SPI bus with a simulated SD card (backed by a FAT image file)
 * and a recorder for the panel traffic. Checks that every frame the player sends equals the frame in
 * VIDEO.BIN. Bus time is counted (bytes * 8 / clock), CPU time is not, so the fps it prints is a ceiling.
 * Build and run: tests/run_sim.sh */

extern int player_main(void);
#include <stdint.h>
extern struct { uint32_t magic; int32_t err; uint32_t fps_target, frames_in_file, frames_shown, late, elapsed_ms, sd_us, lcd_us, bytes_read, fps_x100, sd_kBps_x10, sd_hz, sd_sectors, sdhc, stack_free; int32_t fr; } player_res;

#define SIM_DONE() (player_res.magic == 0x504C4102 || player_res.err)
#include "sim_port.h"

static uint8_t rev(uint8_t b) { uint8_t r = 0; int i; for (i = 0; i < 8; i++) if (b & (1 << i)) r |= (uint8_t)(0x80 >> i); return r; }

int main(int argc, char **argv)
{
    FILE *vf;
    uint8_t *want; long vsz; uint32_t frames, k, ok = 0, bad = 0, rows_seen = 0, p;
    (void)rows_seen;
    static uint8_t cur[2048];
    int rowmask[128] = {0}, nrows = 0;
    if (argc < 3) { fprintf(stderr, "usage: sim image VIDEO.BIN\n"); return 2; }
    img = fopen(argv[1], "rb"); vf = fopen(argv[2], "rb");
    if (!img || !vf) { perror("open"); return 2; }
    fseek(vf, 0, SEEK_END); vsz = ftell(vf); fseek(vf, 0, SEEK_SET);
    want = malloc(vsz); if (fread(want, 1, vsz, vf) != (size_t)vsz) return 2;
    frames = (uint32_t)want[12] | (want[13] << 8) | (want[14] << 16);
    if (!setjmp(done)) player_main();

    /* decode the panel log, one chip-select session at a time: mode byte, rows of (address, 16 data, 0), trailer */
    k = 0; memset(cur, 0, sizeof cur);
    for (p = 0; p < sessions; p++) {
        uint32_t q0 = sess_at[p] + 1, end = (p + 1 < sessions) ? sess_at[p + 1] : lcd_n;
        while (q0 + 18 <= end) {
            uint8_t a = rev(lcd[q0]);
            uint32_t j;
            if (a < 1 || a > 128) break;
            for (j = 0; j < 16; j++) cur[(a - 1) * 16 + j] = rev(lcd[q0 + 1 + j]);
            if (!rowmask[a - 1]) { rowmask[a - 1] = 1; nrows++; }
            q0 += 18; rows_seen++;
        }
        if (nrows == 128) {
            if (k < frames && memcmp(cur, want + 512 + (size_t)k * 2048, 2048) == 0) ok++;
            else { bad++; if (bad < 4) printf("frame %u differs\n", k); }
            k++; memset(rowmask, 0, sizeof rowmask); nrows = 0;
        }
    }
    printf("card: %u commands, %u blocks, %u panel sessions\n", card_cmds, card_blocks, sessions);
    printf("player: err %d (fr %d), shown %u of %u, late %u, elapsed %u ms (bus time only), sd %u kB/s x10, fps x100 %u, mount sectors %u\n",
           player_res.err, player_res.fr, player_res.frames_shown, player_res.frames_in_file, player_res.late, player_res.elapsed_ms, player_res.sd_kBps_x10, player_res.fps_x100, player_res.sd_sectors);
    printf("frames checked: %u equal, %u different, %u expected\n", ok, bad, frames);
    printf(player_res.err == 0 && bad == 0 && ok == frames ? "PASS\n" : "FAIL\n");
    return !(player_res.err == 0 && bad == 0 && ok == frames);
}
