#include "chip8.h"

const c8_quirks_t c8_quirks_vip    = { 1, 1, 1, 1, 1, 0 };
const c8_quirks_t c8_quirks_modern = { 0, 0, 0, 1, 0, 0 };

static const uint8_t font[80] = {
    0xF0,0x90,0x90,0x90,0xF0, 0x20,0x60,0x20,0x20,0x70, 0xF0,0x10,0xF0,0x80,0xF0, 0xF0,0x10,0xF0,0x10,0xF0,
    0x90,0x90,0xF0,0x10,0x10, 0xF0,0x80,0xF0,0x10,0xF0, 0xF0,0x80,0xF0,0x90,0xF0, 0xF0,0x10,0x20,0x40,0x40,
    0xF0,0x90,0xF0,0x90,0xF0, 0xF0,0x90,0xF0,0x10,0xF0, 0xF0,0x90,0xF0,0x90,0x90, 0xE0,0x90,0xE0,0x90,0xE0,
    0xF0,0x80,0x80,0x80,0xF0, 0xE0,0x90,0x90,0x90,0xE0, 0xF0,0x80,0xF0,0x80,0xF0, 0xF0,0x80,0xF0,0x80,0x80 };

void c8_reset(chip8_t *c, const c8_quirks_t *q)
{
    uint8_t *p = (uint8_t *)c; for (unsigned n = 0; n < sizeof *c; n++) p[n] = 0;
    for (int n = 0; n < 80; n++) c->mem[0x50 + n] = font[n];
    c->pc = 0x200; c->wait_reg = -1; c->wait_key = -1; c->rng = 0x2545F491u; c->q = *q; c->dirty = 0xFFFFFFFFu;
}

void c8_load(chip8_t *c, const uint8_t *rom, unsigned n)
{
    if (n > 4096 - 0x200) n = 4096 - 0x200;
    for (unsigned k = 0; k < n; k++) c->mem[0x200 + k] = rom[k];
}

static uint8_t rnd(chip8_t *c) { c->rng ^= c->rng << 13; c->rng ^= c->rng >> 17; c->rng ^= c->rng << 5; return (uint8_t)(c->rng >> 8); }

/* Returns 1 if a pixel went from on to off (collision). */
static void draw(chip8_t *c, int x0, int y0, int n)
{
    int vf = 0;
    x0 &= C8_W - 1; y0 &= C8_H - 1;
    for (int r = 0; r < n; r++) {
        int y = y0 + r;
        if (y >= C8_H) { if (c->q.clip) break; y &= C8_H - 1; }
        uint8_t bits = c->mem[(c->i + r) & 0xFFF];
        for (int k = 0; k < 8; k++) {
            if (!(bits & (0x80 >> k))) continue;
            int x = x0 + k;
            if (x >= C8_W) { if (c->q.clip) break; x &= C8_W - 1; }
            uint8_t *px = &c->fb[y * C8_W + x];
            if (*px) vf = 1;
            *px ^= 1;
        }
        c->dirty |= 1u << y;
    }
    c->v[15] = (uint8_t)vf;
}

void c8_step(chip8_t *c)
{
    if (c->wait_reg >= 0) {                      /* Fx0A: take the key when it is pressed and let go */
        if (c->wait_key < 0) { for (int k = 0; k < 16; k++) if (c->key[k]) { c->wait_key = (int8_t)k; break; } }
        else if (!c->key[c->wait_key]) { c->v[c->wait_reg] = (uint8_t)c->wait_key; c->wait_reg = -1; c->wait_key = -1; }
        return;
    }
    uint16_t op = (uint16_t)(c->mem[c->pc & 0xFFF] << 8 | c->mem[(c->pc + 1) & 0xFFF]);
    int x = (op >> 8) & 15, y = (op >> 4) & 15, n = op & 15, nn = op & 0xFF, nnn = op & 0xFFF;
    uint8_t *V = c->v;
    c->pc = (c->pc + 2) & 0xFFF; c->steps++;
    switch (op >> 12) {
    case 0:
        if (op == 0x00E0) { for (int k = 0; k < C8_W * C8_H; k++) c->fb[k] = 0; c->dirty = 0xFFFFFFFFu; }
        else if (op == 0x00EE) { if (c->sp) c->pc = c->stack[--c->sp]; }
        break;
    case 1: c->pc = (uint16_t)nnn; break;
    case 2: if (c->sp < 16) c->stack[c->sp++] = c->pc; c->pc = (uint16_t)nnn; break;
    case 3: if (V[x] == nn) c->pc += 2; break;
    case 4: if (V[x] != nn) c->pc += 2; break;
    case 5: if (n == 0 && V[x] == V[y]) c->pc += 2; break;
    case 6: V[x] = (uint8_t)nn; break;
    case 7: V[x] = (uint8_t)(V[x] + nn); break;
    case 8: {
        uint8_t a = V[x], b = V[y], f = 0, r = a; int set = 1;
        switch (n) {
        case 0: r = b; break;
        case 1: r = a | b; if (c->q.vf_reset) { f = 0; set = 2; } break;
        case 2: r = a & b; if (c->q.vf_reset) { f = 0; set = 2; } break;
        case 3: r = a ^ b; if (c->q.vf_reset) { f = 0; set = 2; } break;
        case 4: { unsigned s = a + b; r = (uint8_t)s; f = s > 255; set = 2; } break;
        case 5: r = (uint8_t)(a - b); f = a >= b; set = 2; break;
        case 6: { uint8_t s = c->q.shift_vy ? b : a; r = s >> 1; f = s & 1; set = 2; } break;
        case 7: r = (uint8_t)(b - a); f = b >= a; set = 2; break;
        case 0xE: { uint8_t s = c->q.shift_vy ? b : a; r = (uint8_t)(s << 1); f = s >> 7; set = 2; } break;
        default: set = 0; break;
        }
        if (set) { V[x] = r; if (set == 2) V[15] = f; }       /* VF is written after the result, so VF as VX gets the flag */
    } break;
    case 9: if (n == 0 && V[x] != V[y]) c->pc += 2; break;
    case 0xA: c->i = (uint16_t)nnn; break;
    case 0xB: c->pc = (uint16_t)((nnn + V[c->q.jump_vx ? x : 0]) & 0xFFF); break;
    case 0xC: V[x] = rnd(c) & nn; break;
    case 0xD:
        if (c->q.disp_wait && !c->vblank) { c->pc -= 2; c->steps--; break; }      /* try again after the next frame tick */
        c->vblank = 0; draw(c, V[x], V[y], n); break;
    case 0xE:
        if (nn == 0x9E) { if (c->key[V[x] & 15]) c->pc += 2; }
        else if (nn == 0xA1) { if (!c->key[V[x] & 15]) c->pc += 2; }
        break;
    case 0xF:
        switch (nn) {
        case 0x07: V[x] = c->dt; break;
        case 0x0A: c->wait_reg = (int8_t)x; c->wait_key = -1; break;
        case 0x15: c->dt = V[x]; break;
        case 0x18: c->st = V[x]; break;
        case 0x1E: c->i = (uint16_t)((c->i + V[x]) & 0xFFF); break;
        case 0x29: c->i = (uint16_t)(0x50 + (V[x] & 15) * 5); break;
        case 0x33: c->mem[c->i & 0xFFF] = V[x] / 100; c->mem[(c->i + 1) & 0xFFF] = (V[x] / 10) % 10; c->mem[(c->i + 2) & 0xFFF] = V[x] % 10; break;
        case 0x55: for (int k = 0; k <= x; k++) c->mem[(c->i + k) & 0xFFF] = V[k]; if (c->q.mem_inc) c->i = (uint16_t)((c->i + x + 1) & 0xFFF); break;
        case 0x65: for (int k = 0; k <= x; k++) V[k] = c->mem[(c->i + k) & 0xFFF]; if (c->q.mem_inc) c->i = (uint16_t)((c->i + x + 1) & 0xFFF); break;
        }
        break;
    }
}

void c8_frame(chip8_t *c, int steps)
{
    if (c->dt) c->dt--;
    if (c->st) c->st--;
    c->vblank = 1;
    for (int k = 0; k < steps; k++) c8_step(c);
}
