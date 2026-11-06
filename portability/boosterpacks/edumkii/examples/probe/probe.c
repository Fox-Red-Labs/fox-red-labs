/* Stack check for the Hey Sparky face demo: what is on the LP-MSPM33C321A's BoosterPack headers?
 * Prints once a second on the console (115200): which I2C addresses answer (the Educational BoosterPack's OPT3001 is 0x44,
 * the Audio BoosterPack's has none), the microphone pin (BoosterPack pin 6, PA22) mean and peak-to-peak over 256 reads,
 * joystick and accelerometer counts, S1 and S2. Only reads; drives no pin the demo does not drive (never PA21).
 * BSD-3-Clause. */
#include "ti_msp_dl_config.h"
#include "board_config.h"
#include "edumkii.h"
#include "edu_board.h"

static void put_int(int32_t v)
{
    char b[12]; int n = 0;
    uint32_t u = v < 0 ? (uint32_t)(-(int64_t)v) : (uint32_t)v;
    do { b[n++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    if (v < 0) b[n++] = '-';
    char o[13]; int k = 0;
    while (n) o[k++] = b[--n];
    o[k] = 0;
    edu_print(o);
}

int main(void)
{
    static edumkii_t d;
    SYSCFG_DL_init();
    edu_board_init();
    edumkii_init(&d, edu_port());
    edu_print("\r\nprobe on " EDU_BOARD_NAME "\r\n");
    for (;;) {
        edu_print("i2c:");
        for (uint8_t a = 0x08; a < 0x78; a++) {
            uint8_t z = 0;
            if (d.port.i2c_write(d.port.ctx, a, &z, 0) == 0) { edu_print(" 0x"); put_int(a); }
        }
        uint32_t sum = 0; uint16_t lo = 4095, hi = 0;
        for (int i = 0; i < 256; i++) {
            uint16_t v = edumkii_adc(&d, EDUMKII_ADC_MIC);
            sum += v; if (v < lo) lo = v; if (v > hi) hi = v;
            d.port.delay_us(d.port.ctx, 100);
        }
        edu_print("  mic mean "); put_int((int32_t)(sum / 256u)); edu_print(" pp "); put_int(hi - lo);
        uint16_t jx = edumkii_adc(&d, EDUMKII_ADC_JOY_X), jy = edumkii_adc(&d, EDUMKII_ADC_JOY_Y);
        uint16_t ax, ay, az; edumkii_accel_read(&d, &ax, &ay, &az);
        edu_print("  joy "); put_int(jx); edu_print(" "); put_int(jy);
        edu_print("  acc "); put_int(ax); edu_print(" "); put_int(ay); edu_print(" "); put_int(az);
        edu_print("  S1 "); put_int(edumkii_button(&d, EDUMKII_PIN_S1));
        edu_print(" S2 "); put_int(edumkii_button(&d, EDUMKII_PIN_S2));
        edu_print("\r\n");
        d.port.delay_ms(d.port.ctx, 700);
    }
}
