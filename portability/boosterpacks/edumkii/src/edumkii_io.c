/* Inputs and outputs of the Educational BoosterPack MKII: joystick, buttons, buzzer, LEDs,
 * accelerometer, microphone level, OPT3001. BSD-3-Clause. */
#include "edumkii.h"

int edumkii_init(edumkii_t *d, const edumkii_port_t *port)
{
    if (!port || !port->delay_us || !port->delay_ms) return EDUMKII_ERR_PORT;
    d->port = *port;
    d->joy_x0 = d->joy_y0 = 2048;
    if (d->port.gpio_write) {
        d->port.gpio_write(d->port.ctx, EDUMKII_PIN_LCD_CS, 1);
        d->port.gpio_write(d->port.ctx, EDUMKII_PIN_LCD_DC, 1);
        d->port.gpio_write(d->port.ctx, EDUMKII_PIN_LCD_RST, 1);
        d->port.gpio_write(d->port.ctx, EDUMKII_PIN_BL_RED, 0);
        d->port.gpio_write(d->port.ctx, EDUMKII_PIN_LED_GREEN, 0);
        d->port.gpio_write(d->port.ctx, EDUMKII_PIN_LED_BLUE, 0);
        d->port.gpio_write(d->port.ctx, EDUMKII_PIN_BUZZER, 0);
    }
    return EDUMKII_OK;
}

uint16_t edumkii_adc(edumkii_t *d, edumkii_adc_t ch)
{
    return d->port.adc_read ? d->port.adc_read(d->port.ctx, ch) : 0;
}

int edumkii_button(edumkii_t *d, edumkii_pin_t pin)
{
    if (!d->port.gpio_read) return 0;
    return d->port.gpio_read(d->port.ctx, pin) ? 0 : 1;
}

void edumkii_joy_calibrate(edumkii_t *d)
{
    uint32_t sx = 0, sy = 0;
    for (int i = 0; i < 16; i++) {
        sx += edumkii_adc(d, EDUMKII_ADC_JOY_X);
        sy += edumkii_adc(d, EDUMKII_ADC_JOY_Y);
    }
    d->joy_x0 = (uint16_t)(sx / 16u);
    d->joy_y0 = (uint16_t)(sy / 16u);
}

int edumkii_joy_read(edumkii_t *d, int *dx, int *dy)
{
    if (!d->port.adc_read) return EDUMKII_ERR_PORT;
    *dx = (int)edumkii_adc(d, EDUMKII_ADC_JOY_X) - (int)d->joy_x0;
    *dy = (int)edumkii_adc(d, EDUMKII_ADC_JOY_Y) - (int)d->joy_y0;
    return EDUMKII_OK;
}

int edumkii_joy_dir(edumkii_t *d, int thresh)
{
    int dx, dy, m = 0;
    if (edumkii_joy_read(d, &dx, &dy) != EDUMKII_OK) return 0;
    if (dx > thresh) m |= EDUMKII_DIR_RIGHT;
    if (dx < -thresh) m |= EDUMKII_DIR_LEFT;
    if (dy > thresh) m |= EDUMKII_DIR_UP;      /* raw Y rises as the stick is pushed away from the player (by hand, 2026-10-23) */
    if (dy < -thresh) m |= EDUMKII_DIR_DOWN;
    return m;
}

void edumkii_accel_read(edumkii_t *d, uint16_t *x, uint16_t *y, uint16_t *z)
{
    *x = edumkii_adc(d, EDUMKII_ADC_ACC_X);
    *y = edumkii_adc(d, EDUMKII_ADC_ACC_Y);
    *z = edumkii_adc(d, EDUMKII_ADC_ACC_Z);
}

uint16_t edumkii_mic_level(edumkii_t *d, int n)
{
    uint16_t lo = 0xFFFF, hi = 0;
    if (!d->port.adc_read) return 0;
    for (int i = 0; i < n; i++) {
        uint16_t v = d->port.adc_read(d->port.ctx, EDUMKII_ADC_MIC);
        if (v < lo) lo = v;
        if (v > hi) hi = v;
        d->port.delay_us(d->port.ctx, 100);
    }
    return (uint16_t)(hi - lo);
}

void edumkii_led(edumkii_t *d, int r, int g, int b)
{
    if (!d->port.gpio_write) return;
    d->port.gpio_write(d->port.ctx, EDUMKII_PIN_BL_RED, r ? 1 : 0);
    d->port.gpio_write(d->port.ctx, EDUMKII_PIN_LED_GREEN, g ? 1 : 0);
    d->port.gpio_write(d->port.ctx, EDUMKII_PIN_LED_BLUE, b ? 1 : 0);
}

void edumkii_tone(edumkii_t *d, uint32_t freq_hz, uint32_t ms)
{
    if (freq_hz == 0) { d->port.delay_ms(d->port.ctx, ms); return; }
    if (d->port.pwm_set) {
        d->port.pwm_set(d->port.ctx, freq_hz, 50);
        d->port.delay_ms(d->port.ctx, ms);
        d->port.pwm_set(d->port.ctx, 0, 0);
        return;
    }
    if (!d->port.gpio_write) return;
    uint32_t half = 500000u / freq_hz;               /* microseconds per half period */
    uint32_t n = (ms * 1000u) / (half ? half : 1u);  /* half periods */
    for (uint32_t i = 0; i < n; i++) {
        d->port.gpio_write(d->port.ctx, EDUMKII_PIN_BUZZER, (int)(i & 1u));
        d->port.delay_us(d->port.ctx, half);
    }
    d->port.gpio_write(d->port.ctx, EDUMKII_PIN_BUZZER, 0);
}

static int opt_read_reg(edumkii_t *d, uint8_t reg, uint16_t *v)
{
    uint8_t b[2];
    if (!d->port.i2c_write || !d->port.i2c_read) return EDUMKII_ERR_PORT;
    if (d->port.i2c_write(d->port.ctx, EDUMKII_OPT3001_ADDR, &reg, 1)) return EDUMKII_ERR_BUS;
    if (d->port.i2c_read(d->port.ctx, EDUMKII_OPT3001_ADDR, b, 2)) return EDUMKII_ERR_BUS;
    *v = (uint16_t)((b[0] << 8) | b[1]);
    return EDUMKII_OK;
}

int edumkii_lux_init(edumkii_t *d)
{
    uint16_t id;
    int e = opt_read_reg(d, 0x7E, &id);               /* manufacturer ID, 0x5449 */
    if (e) return e;
    if (id != 0x5449u) return EDUMKII_ERR_BUS;
    e = opt_read_reg(d, 0x7F, &id);                   /* device ID, 0x3001 */
    if (e) return e;
    if (id != 0x3001u) return EDUMKII_ERR_BUS;
    /* config 0x01: automatic range (1100), 800 ms (1), continuous (11), latched (bit 4) */
    const uint8_t cfg[3] = { 0x01, 0xCE, 0x10 };
    return d->port.i2c_write(d->port.ctx, EDUMKII_OPT3001_ADDR, cfg, 3) ? EDUMKII_ERR_BUS : EDUMKII_OK;
}

int edumkii_lux_read(edumkii_t *d, uint32_t *centilux)
{
    uint16_t r;
    int e = opt_read_reg(d, 0x00, &r);
    if (e) return e;
    *centilux = (uint32_t)(r & 0x0FFFu) << (r >> 12);   /* 0.01 lux per count at exponent 0 */
    return EDUMKII_OK;
}
