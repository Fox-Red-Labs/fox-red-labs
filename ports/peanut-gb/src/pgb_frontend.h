/* Peanut-GB front end over the Fox Red Labs abstract display and input (Fox Red Labs, BSD-3-Clause). */
#ifndef PGB_FRONTEND_H
#define PGB_FRONTEND_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bytes the front end needs on top of the ROM (see pgb_state_size): the console state, cartridge RAM
 * and the packed picture. */
size_t pgb_state_size(void);

/* Check the ROM header bytes that Peanut-GB uses as table indexes (ROM size code at 0x148 must be 0 to 8, RAM size
 * code at 0x149 must be 0 to 5) and that the ROM reaches past the header. Returns 0 if fine, -4 too short, -5 bad ROM
 * size code, -6 bad RAM size code. pgb_init calls it first. */
int pgb_check_rom(const uint8_t *rom, size_t rom_len);

/* A one-line message for a pgb_init or pgb_check_rom result (0 gives "ok"). */
const char *pgb_error_text(int code);

/* Start the emulator. rom points at the ROM image (flash or memory-mapped QSPI), read in place.
 * cart_ram/cart_ram_cap hold the cartridge RAM (up to 32 KB; 8 KB covers most games). Returns 0 on
 * success, a negative number if the ROM is not supported, fails pgb_check_rom or the buffer is too small. */
int pgb_init(const uint8_t *rom, size_t rom_len, uint8_t *cart_ram, size_t cart_ram_cap);

/* Read the stick and buttons (A is A, B is B, A and B together press Start, A and B with the stick
 * held down press Select), run one Game Boy frame (about 16.7 ms of game time). */
void pgb_run_frame(void);

/* Same, but without reading input: for a benchmark of the emulator alone. */
void pgb_run_frame_no_input(void);

/* Scale the last frame to the 128 x 128 screen and send it. On an RGB565 display the scaling and the sending already
 * happened line by line while the frame ran (see draw_line_fast), and this only ends the frame; on a skipped frame it does nothing. */
void pgb_present(void);

/* Draw one frame in two (the game still runs every frame, the picture is built and sent every other one). */
void pgb_set_frame_skip(int on);

/* Frames the game has finished since pgb_init. */
uint32_t pgb_frames(void);

/* 1 if the game drew anything other than a blank screen on the last frame. */
int pgb_lcd_on(void);

#ifdef __cplusplus
}
#endif
#endif
