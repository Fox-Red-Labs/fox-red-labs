/* Arduboy2-compatible layer (Fox Red Labs, BSD-3-Clause). See include/Arduboy2.h. */
#include <Arduboy2.h>

uint8_t frl_eeprom_image[FRL_EEPROM_SIZE];
EEPROMClass EEPROM;

/* ---- Arduino time and random ------------------------------------------- */

extern "C" {

unsigned long millis(void)
{
    const frl_platform_t *p = frl_platform();
    return (p && p->millis) ? p->millis(p->clock_ctx) : 0;
}
unsigned long micros(void) { return millis() * 1000UL; }
void delay(unsigned long ms)
{
    const frl_platform_t *p = frl_platform();
    if (p && p->delay_ms) p->delay_ms(p->clock_ctx, (uint32_t)ms);
}
void delayMicroseconds(unsigned int) {}

static uint32_t g_rng = 0x2545F491u;
void randomSeed(unsigned long seed) { g_rng = seed ? (uint32_t)seed : 0x2545F491u; }
static uint32_t rnd32(void)
{
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}
long frl_random_max(long max) { return max > 0 ? (long)(rnd32() % (uint32_t)max) : 0; }
long frl_random_range(long lo, long hi) { return lo >= hi ? lo : lo + frl_random_max(hi - lo); }
long map(long x, long a, long b, long c, long d) { return (x - a) * (d - c) / (b - a) + c; }

} /* extern "C" */

/* ---- Print ------------------------------------------------------------- */

static size_t print_unsigned(Print *p, unsigned long v, int base)
{
    char tmp[34]; int n = 0;
    if (base < 2) base = 10;
    if (v == 0) tmp[n++] = '0';
    while (v) { unsigned d = (unsigned)(v % (unsigned)base); tmp[n++] = (char)(d < 10 ? '0' + d : 'A' + d - 10); v /= (unsigned)base; }
    size_t k = 0;
    while (n) k += p->write((uint8_t)tmp[--n]);
    return k;
}
size_t Print::print(unsigned long v, int base) { return print_unsigned(this, v, base); }
size_t Print::print(long v, int base)
{
    if (base == 10 && v < 0) return write((uint8_t)'-') + print_unsigned(this, (unsigned long)(-v), 10);
    return print_unsigned(this, (unsigned long)v, base);
}
size_t Print::print(double v, int digits)
{
    size_t k = 0;
    if (v < 0) { k += write((uint8_t)'-'); v = -v; }
    double rounding = 0.5;
    for (int i = 0; i < digits; i++) rounding /= 10.0;
    v += rounding;
    unsigned long ip = (unsigned long)v;
    double rem = v - (double)ip;
    k += print_unsigned(this, ip, 10);
    if (digits > 0) {
        k += write((uint8_t)'.');
        while (digits-- > 0) { rem *= 10.0; unsigned d = (unsigned)rem; k += write((uint8_t)('0' + d)); rem -= d; }
    }
    return k;
}

/* ---- Audio and tones --------------------------------------------------- */

static bool g_audio_on = true;
void Arduboy2Audio::on() { g_audio_on = true; }
void Arduboy2Audio::off()
{
    g_audio_on = false;
    const frl_platform_t *p = frl_platform();
    if (p && p->audio.tone) p->audio.tone(p->audio.ctx, 0, 0);
}
bool Arduboy2Audio::enabled() { return g_audio_on; }

static void beep_out(uint16_t count, uint8_t dur)
{
    const frl_platform_t *p = frl_platform();
    if (!g_audio_on || !p || !p->audio.tone) return;
    uint16_t hz = (uint16_t)(1000000UL / ((unsigned long)count + 1UL));
    uint16_t ms = dur ? (uint16_t)(dur * Arduboy2Base::eachFrameMillis) : 0;
    p->audio.tone(p->audio.ctx, hz, ms);
}
void BeepPin1::tone(uint16_t count) { beep_out(count, 0); }
void BeepPin1::tone(uint16_t count, uint8_t dur) { beep_out(count, dur); }
void BeepPin1::noTone() { const frl_platform_t *p = frl_platform(); if (p && p->audio.tone) p->audio.tone(p->audio.ctx, 0, 0); }
void BeepPin2::tone(uint16_t count) { beep_out(count, 0); }
void BeepPin2::tone(uint16_t count, uint8_t dur) { beep_out(count, dur); }
void BeepPin2::noTone() { BeepPin1::noTone(); }

/* ---- Arduboy2Base ------------------------------------------------------ */

uint8_t Arduboy2Base::sBuffer[(HEIGHT * WIDTH) / 8];
uint16_t Arduboy2Base::frameCount = 0;
uint8_t Arduboy2Base::eachFrameMillis = 16;
uint8_t Arduboy2Base::thisFrameStart = 0;
bool Arduboy2Base::justRendered = false;
uint8_t Arduboy2Base::lastFrameDurationMs = 0;
uint8_t Arduboy2Base::currentButtonState = 0;
uint8_t Arduboy2Base::previousButtonState = 0;

void Arduboy2Base::begin()
{
    Arduboy2Audio::on();
    buttonsState();                  /* learns the stick centre while it rests */
    memset(sBuffer, 0, sizeof sBuffer);
    thisFrameStart = (uint8_t)millis();
    display();
}

void Arduboy2Base::waitNoButtons()
{
    do { delay(50); } while (buttonsState());
}

static bool g_all_on = false;
void Arduboy2Base::allPixelsOn(bool on) { g_all_on = on; }

uint8_t Arduboy2Base::buttonsState() { return frl_read_buttons(); }
void Arduboy2Base::pollButtons()
{
    previousButtonState = currentButtonState;
    currentButtonState = buttonsState();
}

bool Arduboy2Base::nextFrame()
{
    uint8_t now = (uint8_t)millis();
    uint8_t frameDurationMs = (uint8_t)(now - thisFrameStart);
    if (justRendered) {
        lastFrameDurationMs = frameDurationMs;
        justRendered = false;
        return false;
    } else if (frameDurationMs < eachFrameMillis) {
        if (++frameDurationMs < eachFrameMillis) idle();
        return false;
    }
    justRendered = true;
    thisFrameStart = now;
    frameCount++;
    return true;
}

unsigned long Arduboy2Base::generateRandomSeed()
{
    const frl_platform_t *p = frl_platform();
    uint32_t e = (p && p->entropy) ? p->entropy(p->clock_ctx) : (uint32_t)millis();
    return e ^ (uint32_t)millis() * 2654435761u;
}
void Arduboy2Base::initRandomSeed() { randomSeed(generateRandomSeed()); }

void Arduboy2Base::display()
{
    if (g_all_on) { static uint8_t full[sizeof sBuffer]; memset(full, 0xFF, sizeof full); frl_present_pages(full, WIDTH, HEIGHT, frl_get_fit()); return; }
    frl_present_pages(sBuffer, WIDTH, HEIGHT, frl_get_fit());
}
void Arduboy2Base::display(bool clr) { display(); if (clr) clear(); }
void Arduboy2Base::clear() { fillScreen(BLACK); }
void Arduboy2Base::fillScreen(uint8_t color) { memset(sBuffer, color ? 0xFF : 0x00, sizeof sBuffer); }

void Arduboy2Base::drawPixel(int16_t x, int16_t y, uint8_t color)
{
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    uint8_t *p = &sBuffer[(y >> 3) * WIDTH + x];
    uint8_t m = (uint8_t)(1u << (y & 7));
    if (color == WHITE) *p |= m;
    else if (color == BLACK) *p &= (uint8_t)~m;
    else *p ^= m;
}

uint8_t Arduboy2Base::getPixel(uint8_t x, uint8_t y)
{
    if (x >= WIDTH || y >= HEIGHT) return 0;
    return (sBuffer[(y >> 3) * WIDTH + x] >> (y & 7)) & 1;
}

void Arduboy2Base::drawFastVLine(int16_t x, int16_t y, uint8_t h, uint8_t color)
{
    int16_t end = y + h;
    if (x < 0 || x >= WIDTH) return;
    if (y < 0) y = 0;
    if (end > HEIGHT) end = HEIGHT;
    for (; y < end; y++) drawPixel(x, y, color);
}

void Arduboy2Base::drawFastHLine(int16_t x, int16_t y, uint8_t w, uint8_t color)
{
    int16_t end = x + w;
    if (y < 0 || y >= HEIGHT) return;
    if (x < 0) x = 0;
    if (end > WIDTH) end = WIDTH;
    if (color == INVERT) { for (; x < end; x++) drawPixel(x, y, INVERT); return; }
    uint8_t *row = &sBuffer[(y >> 3) * WIDTH];
    uint8_t m = (uint8_t)(1u << (y & 7));
    if (color == WHITE) for (; x < end; x++) row[x] |= m;
    else for (; x < end; x++) row[x] &= (uint8_t)~m;
}

void Arduboy2Base::drawRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t color)
{
    if (!w || !h) return;
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, y + h - 1, w, color);
    drawFastVLine(x, y, h, color);
    drawFastVLine(x + w - 1, y, h, color);
}

void Arduboy2Base::fillRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t color)
{
    for (int16_t i = x; i < x + w; i++) drawFastVLine(i, y, h, color);
}

void Arduboy2Base::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
{
    bool steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) { int16_t t = x0; x0 = y0; y0 = t; t = x1; x1 = y1; y1 = t; }
    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    int16_t dx = x1 - x0, dy = abs(y1 - y0);
    int16_t err = dx / 2;
    int16_t ystep = y0 < y1 ? 1 : -1;
    for (; x0 <= x1; x0++) {
        if (steep) drawPixel(y0, x0, color); else drawPixel(x0, y0, color);
        err -= dy;
        if (err < 0) { y0 += ystep; err += dx; }
    }
}

void Arduboy2Base::drawCircle(int16_t x0, int16_t y0, uint8_t r, uint8_t color)
{
    int16_t x = 0, y = r, d = 1 - r;
    while (x <= y) {
        drawPixel(x0 + x, y0 + y, color); drawPixel(x0 - x, y0 + y, color);
        drawPixel(x0 + x, y0 - y, color); drawPixel(x0 - x, y0 - y, color);
        drawPixel(x0 + y, y0 + x, color); drawPixel(x0 - y, y0 + x, color);
        drawPixel(x0 + y, y0 - x, color); drawPixel(x0 - y, y0 - x, color);
        if (d < 0) d += 2 * x + 3; else { d += 2 * (x - y) + 5; y--; }
        x++;
    }
}

void Arduboy2Base::fillCircle(int16_t x0, int16_t y0, uint8_t r, uint8_t color)
{
    int16_t x = 0, y = r, d = 1 - r;
    while (x <= y) {
        drawFastHLine(x0 - y, y0 + x, (uint8_t)(2 * y + 1), color);
        drawFastHLine(x0 - y, y0 - x, (uint8_t)(2 * y + 1), color);
        if (d < 0) { d += 2 * x + 3; }
        else {
            drawFastHLine(x0 - x, y0 + y, (uint8_t)(2 * x + 1), color);
            drawFastHLine(x0 - x, y0 - y, (uint8_t)(2 * x + 1), color);
            d += 2 * (x - y) + 5; y--;
        }
        x++;
    }
}

void Arduboy2Base::drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t w, uint8_t h, uint8_t color)
{
    if (x + w < 0 || x >= WIDTH || y + h < 0 || y >= HEIGHT) return;
    uint8_t pages = (uint8_t)((h + 7) / 8);
    for (uint8_t p = 0; p < pages; p++) {
        for (uint8_t i = 0; i < w; i++) {
            uint8_t b = pgm_read_byte(bitmap + p * w + i);
            if (!b) continue;
            for (uint8_t k = 0; k < 8 && (p * 8 + k) < h; k++)
                if (b & (1u << k)) drawPixel(x + i, y + p * 8 + k, color);
        }
    }
}

void Arduboy2Base::drawSlowXYBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t w, uint8_t h, uint8_t color)
{
    uint8_t bytes = (uint8_t)((w + 7) / 8);
    for (uint8_t j = 0; j < h; j++)
        for (uint8_t i = 0; i < w; i++)
            if (pgm_read_byte(bitmap + j * bytes + i / 8) & (0x80 >> (i & 7))) drawPixel(x + i, y + j, color);
}

bool Arduboy2Base::collide(Point p, Rect r)
{
    return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
}
bool Arduboy2Base::collide(Rect a, Rect b)
{
    return a.x < b.x + b.width && a.x + a.width > b.x && a.y < b.y + b.height && a.y + a.height > b.y;
}

/* ---- Arduboy2 text ------------------------------------------------------ */

const uint8_t Arduboy2::font5x7[] PROGMEM = {
#include "font5x7.inc"
};

void Arduboy2::clear() { Arduboy2Base::clear(); cursor_x = 0; cursor_y = 0; }

size_t Arduboy2::write(uint8_t c)
{
    if (c == '\r') return 1;
    if (c == '\n' || (textWrap && cursor_x > (WIDTH - characterWidth * textSize))) {
        cursor_x = 0;
        cursor_y += fullCharacterHeight * textSize;
    }
    if (c != '\n') {
        drawChar(cursor_x, cursor_y, c, textColor, textBackground, textSize);
        cursor_x += fullCharacterWidth * textSize;
    }
    return 1;
}

void Arduboy2::drawChar(int16_t x, int16_t y, uint8_t c, uint8_t color, uint8_t bg, uint8_t size)
{
    bool draw_bg = bg != color;
    const uint8_t *glyph = &font5x7[c * characterWidth];
    for (uint8_t i = 0; i < fullCharacterWidth; i++) {
        uint8_t col = (i < characterWidth) ? pgm_read_byte(glyph + i) : 0;
        for (uint8_t j = 0; j < fullCharacterHeight; j++) {
            bool on = (j < characterHeight) && (col & 1);
            if (on || draw_bg) {
                if (size == 1) drawPixel(x + i, y + j, on ? color : bg);
                else fillRect(x + i * size, y + j * size, size, size, on ? color : bg);
            }
            col >>= 1;
        }
    }
}

/* ---- Sprites ------------------------------------------------------------ */

void Sprites::drawExternalMask(int16_t x, int16_t y, const uint8_t *bitmap, const uint8_t *mask, uint8_t frame, uint8_t mask_frame)
{ draw(x, y, bitmap, frame, mask, mask_frame, SPRITE_MASKED); }
void Sprites::drawOverwrite(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame)
{ draw(x, y, bitmap, frame, 0, 0, SPRITE_OVERWRITE); }
void Sprites::drawErase(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame)
{ draw(x, y, bitmap, frame, 0, 0, SPRITE_IS_MASK_ERASE); }
void Sprites::drawSelfMasked(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame)
{ draw(x, y, bitmap, frame, 0, 0, SPRITE_IS_MASK); }
void Sprites::drawPlusMask(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame)
{ draw(x, y, bitmap, frame, 0, 0, SPRITE_PLUS_MASK); }

void Sprites::draw(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame, const uint8_t *mask, uint8_t mask_frame, uint8_t mode)
{
    if (!bitmap) return;
    uint8_t w = pgm_read_byte(bitmap), h = pgm_read_byte(bitmap + 1);
    bitmap += 2;
    unsigned frame_size = w * ((h + 7) / 8);
    if (mode == SPRITE_PLUS_MASK) frame_size *= 2;
    else if (mask) mask += mask_frame * frame_size;
    bitmap += frame * frame_size;
    drawBitmap(x, y, bitmap, mask, w, h, mode);
}

void Sprites::drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, const uint8_t *mask, uint8_t w, uint8_t h, uint8_t mode)
{
    if (!bitmap || x + w <= 0 || x >= WIDTH || y + h <= 0 || y >= HEIGHT) return;
    int16_t sRow = y >> 3;                 /* floor(y / 8), also for negative y */
    uint8_t shift = (uint8_t)(y & 7);
    uint8_t pages = (uint8_t)((h + 7) / 8);
    unsigned step = (mode == SPRITE_PLUS_MASK) ? 2 : 1;
    for (uint8_t p = 0; p < pages; p++) {
        for (uint8_t i = 0; i < w; i++) {
            int16_t sx = x + i;
            if (sx < 0 || sx >= WIDTH) continue;
            unsigned idx = ((unsigned)p * w + i) * step;
            uint8_t sprite = pgm_read_byte(bitmap + idx);
            uint8_t m;
            switch (mode) {
            case SPRITE_PLUS_MASK: m = pgm_read_byte(bitmap + idx + 1); break;
            case SPRITE_MASKED:    m = mask ? pgm_read_byte(mask + idx) : 0xFF; break;
            case SPRITE_OVERWRITE: m = 0xFF; break;
            case SPRITE_IS_MASK_ERASE: m = sprite; sprite = 0; break;
            default:               m = sprite; break;   /* SPRITE_IS_MASK: self masked */
            }
            uint16_t sp = (uint16_t)sprite << shift, mk = (uint16_t)m << shift;
            int16_t pg = sRow + p;
            if (pg >= 0 && pg < HEIGHT / 8) {
                uint8_t *d = &Arduboy2Base::sBuffer[pg * WIDTH + sx];
                *d = (uint8_t)((*d & ~(uint8_t)mk) | (uint8_t)sp);
            }
            if (shift && pg + 1 >= 0 && pg + 1 < HEIGHT / 8) {
                uint8_t *d = &Arduboy2Base::sBuffer[(pg + 1) * WIDTH + sx];
                *d = (uint8_t)((*d & ~(uint8_t)(mk >> 8)) | (uint8_t)(sp >> 8));
            }
        }
    }
}
