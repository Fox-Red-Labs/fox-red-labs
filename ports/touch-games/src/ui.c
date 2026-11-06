#include "game.h"
#include "gfx.h"

void button_draw(const button_t *b, uint16_t fill, uint16_t ink)
{
    gfx_fill(b->x, b->y, b->w, b->h, fill);
    gfx_text_center(b->x, b->y + (b->h - 21) / 2, b->w, b->label, 3, ink, fill);
}

int button_hit(const button_t *b, int x, int y)
{
    return x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h;
}
