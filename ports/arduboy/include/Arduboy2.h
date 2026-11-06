/*
 * Arduboy2-compatible API for Arm MCUs and hosts (Fox Red Labs, BSD-3-Clause).
 *
 * Reimplemented from the public API of the Arduboy2 library (BSD-3-Clause,
 * https://github.com/MLXXXp/Arduboy2), not wrapped: the original talks to AVR
 * registers (SPI, timers, ports) and cannot compile for Arm. The one copied piece is the
 * 5x8 font table (src/font5x7.inc, see LICENSE-Arduboy2.txt).
 *
 * Deliberate differences from the original:
 *  - begin() does not play the boot logo or sound (beginNoLogo behaviour).
 *  - Display, buttons, tones and the clock come from frl_set_platform() (frl/abstract.h).
 *  - EEPROM is RAM; audio on/off is not saved.
 *  - Not implemented: drawRoundRect, fillRoundRect, drawTriangle, fillTriangle,
 *    writeShowIndicator, the ArduboyTones and FX classes.
 */
#ifndef FRL_ARDUBOY2_H
#define FRL_ARDUBOY2_H

#include <Arduino.h>
#include <EEPROM.h>
#include <frl/abstract.h>

#define ARDUBOY_LIB_VER 60000

#define WIDTH 128
#define HEIGHT 64
#define WHITE 1
#define BLACK 0
#define INVERT 2

#define LEFT_BUTTON _BV(5)
#define RIGHT_BUTTON _BV(6)
#define UP_BUTTON _BV(7)
#define DOWN_BUTTON _BV(4)
#define A_BUTTON _BV(3)
#define B_BUTTON _BV(2)

#define EEPROM_STORAGE_SPACE_START 16

/* sprite draw modes, for Sprites::drawBitmap-style callers */
#define SPRITE_MASKED 1
#define SPRITE_UNMASKED 2
#define SPRITE_OVERWRITE 2
#define SPRITE_PLUS_MASK 3
#define SPRITE_IS_MASK 250
#define SPRITE_IS_MASK_ERASE 251
#define SPRITE_AUTO_MODE 255

struct Point {
    int16_t x;
    int16_t y;
};

struct Rect {
    int16_t x;
    int16_t y;
    uint8_t width;
    uint8_t height;
    Rect() {}
    Rect(int16_t px, int16_t py, uint8_t w, uint8_t h) : x(px), y(py), width(w), height(h) {}
};

class Arduboy2Audio {
public:
    static void begin() { on(); }
    static void on();
    static void off();
    static void toggle() { if (enabled()) off(); else on(); }
    static void saveOnOff() {}
    static bool enabled();
};

class Arduboy2Base {
public:
    Arduboy2Base() {}
    void begin();
    void beginNoLogo() { begin(); }
    void boot() { begin(); }
    void bootLogo() {}
    void bootLogoCompressed() {}
    void bootLogoSpritesSelfMasked() {}
    void bootLogoText() {}
    void bootLogoExtra() {}
    void flashlight() {}
    void systemButtons() {}
    void safeMode() {}
    void exitToBootloader() {}
    void waitNoButtons();
    void idle() {}
    static void invert(bool) {}
    static void allPixelsOn(bool);
    static void flipVertical(bool) {}
    static void flipHorizontal(bool) {}
    static void setRGBled(uint8_t, uint8_t, uint8_t) {}
    static void setRGBled(uint8_t, uint8_t) {}
    static void digitalWriteRGB(uint8_t, uint8_t, uint8_t) {}
    static void digitalWriteRGB(uint8_t, uint8_t) {}
    static void freeRGBled() {}

    void clear();
    void display();
    void display(bool clear);
    static uint8_t *getBuffer() { return sBuffer; }
    void fillScreen(uint8_t color);
    void drawPixel(int16_t x, int16_t y, uint8_t color = WHITE);
    uint8_t getPixel(uint8_t x, uint8_t y);
    void drawCircle(int16_t x0, int16_t y0, uint8_t r, uint8_t color = WHITE);
    void fillCircle(int16_t x0, int16_t y0, uint8_t r, uint8_t color = WHITE);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color = WHITE);
    void drawFastVLine(int16_t x, int16_t y, uint8_t h, uint8_t color = WHITE);
    void drawFastHLine(int16_t x, int16_t y, uint8_t w, uint8_t color = WHITE);
    void drawRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t color = WHITE);
    void fillRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t color = WHITE);
    void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t w, uint8_t h, uint8_t color = WHITE);
    void drawSlowXYBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t w, uint8_t h, uint8_t color = WHITE);

    void setFrameRate(uint8_t rate) { eachFrameMillis = 1000 / rate; }
    bool nextFrame();
    bool nextFrameDEV() { return nextFrame(); }
    bool everyXFrames(uint8_t frames) { return frameCount % frames == 0; }
    int cpuLoad() { return lastFrameDurationMs * 100 / eachFrameMillis; }
    void initRandomSeed();
    unsigned long generateRandomSeed();

    void pollButtons();
    bool pressed(uint8_t buttons) { return (buttonsState() & buttons) == buttons; }
    bool notPressed(uint8_t buttons) { return (buttonsState() & buttons) == 0; }
    bool anyPressed(uint8_t buttons) { return (buttonsState() & buttons) != 0; }
    bool justPressed(uint8_t button) { return !(previousButtonState & button) && (currentButtonState & button); }
    bool justReleased(uint8_t button) { return (previousButtonState & button) && !(currentButtonState & button); }
    static uint8_t buttonsState();

    static bool collide(Point point, Rect rect);
    static bool collide(Rect a, Rect b);

    Arduboy2Audio audio;

    static uint8_t sBuffer[(HEIGHT * WIDTH) / 8];
    static uint16_t frameCount;
    static uint8_t eachFrameMillis;
    static uint8_t thisFrameStart;
    static bool justRendered;
    static uint8_t lastFrameDurationMs;
    static uint8_t currentButtonState;
    static uint8_t previousButtonState;

    static constexpr uint8_t width() { return WIDTH; }
    static constexpr uint8_t height() { return HEIGHT; }
};

class Arduboy2 : public Print, public Arduboy2Base {
public:
    Arduboy2() {}
    virtual size_t write(uint8_t) override;
    using Print::write;
    void drawChar(int16_t x, int16_t y, uint8_t c, uint8_t color, uint8_t bg, uint8_t size);
    void setCursor(int16_t x, int16_t y) { cursor_x = x; cursor_y = y; }
    int16_t getCursorX() { return cursor_x; }
    int16_t getCursorY() { return cursor_y; }
    void setCursorX(int16_t x) { cursor_x = x; }
    void setCursorY(int16_t y) { cursor_y = y; }
    void setTextColor(uint8_t color) { textColor = color; }
    uint8_t getTextColor() { return textColor; }
    void setTextBackground(uint8_t bg) { textBackground = bg; }
    uint8_t getTextBackground() { return textBackground; }
    void setTextSize(uint8_t s) { textSize = s > 0 ? s : 1; }
    uint8_t getTextSize() { return textSize; }
    void setTextWrap(bool w) { textWrap = w; }
    bool getTextWrap() { return textWrap; }
    void clear();
    static uint8_t textWidthChars(uint8_t n) { return n * fullCharacterWidth; }

    static const uint8_t font5x7[];

protected:
    static constexpr uint8_t characterWidth = 5;
    static constexpr uint8_t characterHeight = 8;
    static constexpr uint8_t characterSpacing = 1;
    static constexpr uint8_t lineSpacing = 1;
    static constexpr uint8_t fullCharacterWidth = characterWidth + characterSpacing;
    static constexpr uint8_t fullCharacterHeight = characterHeight + lineSpacing;
    int16_t cursor_x = 0;
    int16_t cursor_y = 0;
    uint8_t textColor = 1;
    uint8_t textBackground = 0;
    uint8_t textSize = 1;
    bool textWrap = false;
};

class Sprites {
public:
    static void drawExternalMask(int16_t x, int16_t y, const uint8_t *bitmap, const uint8_t *mask,
                                 uint8_t frame, uint8_t mask_frame);
    static void drawOverwrite(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame);
    static void drawErase(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame);
    static void drawSelfMasked(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame);
    static void drawPlusMask(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame);
    static void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, const uint8_t *mask,
                           uint8_t w, uint8_t h, uint8_t draw_mode);
private:
    static void draw(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame, const uint8_t *mask,
                     uint8_t mask_frame, uint8_t mode);
};
typedef Sprites SpritesB;

#define BEEP_FREQ_COUNT(hz) ((uint16_t)(1000000UL / (hz) - 1))
class BeepPin1 {
public:
    static void begin() {}
    static void tone(uint16_t count);
    static void tone(uint16_t count, uint8_t dur);
    static void timer() {}
    static void noTone();
    static uint16_t freq(float hz) { return BEEP_FREQ_COUNT((unsigned long)hz); }
};
class BeepPin2 {
public:
    static void begin() {}
    static void tone(uint16_t count);
    static void tone(uint16_t count, uint8_t dur);
    static void timer() {}
    static void noTone();
    static uint16_t freq(float hz) { return BEEP_FREQ_COUNT((unsigned long)hz); }
};

#endif
