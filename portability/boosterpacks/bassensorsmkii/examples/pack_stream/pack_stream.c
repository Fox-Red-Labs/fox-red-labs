/* pack_stream: the BP-BASSENSORSMKII sensors that answer, as a 100 Hz dataset-stream v1 line for tools/dataset/capture.py.
 *   # dataset-stream v1 rate_hz=100 sensor=drv5055+hdc2080+opt3001
 *   t_ms,hall_mv,temp_cc,lux
 * hall_mv   DRV5055 output in millivolts, read every 10 ms (about 1638 with no magnet; a magnet moves it by tens of mV).
 * temp_cc   HDC2080 temperature in hundredths of a degree C, a new value once a second (held between).
 * lux      OPT3001 light in whole lux (capped at 32767),  continuous mode (100 ms conversions), read every 100 ms (held between).
 * The slow sensors never block the loop: the HDC2080 is triggered in one slot and read in a later one.
 * t_ms is the time the sample was due; a late loop prints "# overrun <n>". Built for dataset use when the BMI160 stays silent. */
#include "bassens.h"
#include "bs_board.h"
#include "bs_print.h"

#define PERIOD_US 10000u
static void comma(void) { bs_console_putc(','); }

int main(void)
{
    bs_board_init();
    bs_power_all(1);
    bs_port_delay_ms(50);
    bs_puts("\r\n# pack_stream: LP-MSPM0G5187 + BP-BASSENSORSMKII\r\n");
    int opt_ok = (bs_opt3001_probe() == 0) && (bs_reg_write16be(BS_ADDR_OPT3001, 0x01, 0xC610) == 0);   /* auto range, 100 ms, continuous */
    bs_puts("# hdc2080="); bs_puti(bs_hdc2080_probe()); bs_puts(" opt3001="); bs_puti(opt_ok ? 0 : -1); bs_puts("\r\n");
    bs_puts("# dataset-stream v1 rate_hz=100 sensor=drv5055+hdc2080+opt3001\r\n");
    bs_puts("t_ms,hall_mv,temp_cc,lux\r\n");
    int32_t temp_cc = 0, lux = 0, hall = 0;
    uint32_t start = bs_port_micros(), n = 0, bad = 0;
    for (;;) {
        uint32_t due = start + n * PERIOD_US;
        while ((int32_t)(bs_port_micros() - due) < 0) { }
        uint32_t late = (bs_port_micros() - due) / PERIOD_US;
        if (late) { bs_puts("# overrun "); bs_putu(late); bs_puts("\r\n"); n += late; }
        uint32_t slot = n % 100u;
        if (slot == 0) bs_reg_write(BS_ADDR_HDC2080, 0x0F, 0x01);                 /* trigger; result is ready in about 2 ms */
        if (slot == 2) {
            uint8_t b[2];
            if (bs_reg_read(BS_ADDR_HDC2080, 0x00, b, 2) == 0) {
                uint32_t t = (uint32_t)(b[1] << 8 | b[0]);
                temp_cc = (int32_t)(t * 16500u / 65536u) - 4000;
            } else bad++;
        }
        if (opt_ok && slot % 10u == 5) {
            uint16_t r;
            if (bs_reg_read16be(BS_ADDR_OPT3001, 0x00, &r) == 0) {
                lux = (int32_t)(((uint32_t)(r & 0x0FFF) << (r >> 12)) / 100u);
                if (lux > 32767) lux = 32767;
            } else bad++;
        }
        int32_t mv;
        if (bs_drv5055_read_mv(&mv) == 0) hall = mv; else bad++;
        bs_putu(n * (PERIOD_US / 1000u)); comma(); bs_puti(hall); comma(); bs_puti(temp_cc); comma(); bs_puti(lux); bs_puts("\r\n");
        n++;
    }
}
