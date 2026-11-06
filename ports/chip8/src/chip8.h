/* CHIP-8 interpreter core: portable C, no hardware, no allocation. Written fresh from the CHIP-8 opcode tables and the
 * quirk descriptions in the chip8-test-suite README. Original (COSMAC VIP) behaviour is the default; the quirks are fields. */
#ifndef CHIP8_H
#define CHIP8_H
#include <stdint.h>

#define C8_W 64
#define C8_H 32

typedef struct {
    uint8_t vf_reset;      /* 8xy1, 8xy2, 8xy3 set VF to 0 */
    uint8_t mem_inc;       /* Fx55 and Fx65 leave I at I + X + 1 */
    uint8_t disp_wait;     /* Dxyn waits for the next 60 Hz frame */
    uint8_t clip;          /* sprites clip at the edges instead of wrapping (the start coordinate always wraps) */
    uint8_t shift_vy;      /* 8xy6 and 8xyE shift VY into VX (off: shift VX) */
    uint8_t jump_vx;       /* Bnnn jumps to nnn + VX (off: nnn + V0) */
} c8_quirks_t;

typedef struct {
    uint8_t  mem[4096];
    uint8_t  fb[C8_W * C8_H];       /* one byte per pixel, 0 or 1 */
    uint8_t  v[16];
    uint16_t i, pc, stack[16];
    uint8_t  sp, dt, st, key[16];
    uint8_t  vblank;                /* a 60 Hz frame started and no sprite has used it yet */
    int8_t   wait_reg;              /* Fx0A: register waiting for a key, -1 none */
    int8_t   wait_key;              /* the key pressed during Fx0A, -1 until one is */
    uint32_t dirty;                 /* bit r set: screen row r changed since the last clear of this mask */
    uint32_t rng;
    uint32_t steps;                 /* instructions run */
    c8_quirks_t q;
} chip8_t;

extern const c8_quirks_t c8_quirks_vip, c8_quirks_modern;
void c8_reset(chip8_t *c, const c8_quirks_t *q);                       /* clear, load the font at 0x50, pc = 0x200 */
void c8_load(chip8_t *c, const uint8_t *rom, unsigned n);              /* copy a program to 0x200 */
void c8_step(chip8_t *c);                                              /* one instruction */
void c8_frame(chip8_t *c, int steps);                                  /* 60 Hz tick (timers, vblank) then up to `steps` instructions */
#endif
