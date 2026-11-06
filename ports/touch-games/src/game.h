/* What the launcher needs from a game. */
#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include "gesture.h"

typedef struct {
    const char *name;
    void (*start)(uint32_t seed);                  /* new game, then draw everything */
    void (*draw_all)(void);                        /* full redraw of the current state */
    int  (*event)(const gest_evt_t *e);            /* returns 1 if the player opened the menu */
    void (*tick)(uint32_t now_ms);                 /* once per main-loop pass; the mine counter timer lives here */
} game_t;

extern const game_t game_2048, game_mines;

/* Shared look: page background, button drawing and hit test. */
#define COL_BG      RGB(250, 248, 239)
#define COL_INK     RGB(119, 110, 101)
typedef struct { int16_t x, y, w, h; const char *label; } button_t;
void button_draw(const button_t *b, uint16_t fill, uint16_t ink);
int  button_hit(const button_t *b, int x, int y);
#endif
