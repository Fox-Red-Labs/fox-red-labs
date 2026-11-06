/* hello: print every sensor of the BP-BASSENSORSMKII once a second on the backchannel UART (115200 8N1).
 * One line:  n=3 scan=40,44,48,69 hdc=24.38C,37.7% opt=296.00lux tmp117=24.352C drv=1.639V(0.0mT) bmi=@69 a=12,-8,1004mg g=0,1,-2mdps t=24.1C mag=-12,40,-88 err=0
 * A sensor that does not answer prints "none" and is retried every cycle, so one that wakes up shows without a reflash. */
#include "bassens.h"
#include "bs_board.h"
#include "bs_print.h"

int main(void)
{
    bs_board_init();
    bs_puts("\r\nbassens hello: BP-BASSENSORSMKII, one line a second\r\n");
    bs_bmi160_t imu = {0};
    int imu_on = 0, mag_on = 0;
    uint32_t errors = 0;
    for (uint32_t n = 1;; n++) {
        uint32_t t0 = bs_port_micros();
        uint8_t found[16];
        int nf = bs_scan(found, 16);
        bs_puts("n="); bs_putu(n); bs_puts(" scan=");
        if (!nf) bs_puts("none");
        for (int i = 0; i < nf && i < 16; i++) { if (i) bs_console_putc(','); bs_puthex(found[i], 2); }

        int32_t c, h, mc, mv;
        uint32_t clux;
        bs_puts(" hdc=");
        if (bs_hdc2080_probe() == 0 && bs_hdc2080_read(&c, &h) == 0) { bs_putfix(c, 100); bs_puts("C,"); bs_putfix(h, 10); bs_puts("%"); } else { bs_puts("none"); errors++; }
        bs_puts(" opt=");
        if (bs_opt3001_probe() == 0 && bs_opt3001_read_clux(&clux) == 0) { bs_putfix((int32_t)clux, 100); bs_puts("lux"); } else { bs_puts("none"); errors++; }
        bs_puts(" tmp117=");
        if (bs_tmp117_probe() == 0 && bs_tmp117_read_mc(&mc) == 0) { bs_putfix(mc, 1000); bs_puts("C"); } else { bs_puts("none"); errors++; }
        bs_puts(" drv=");
        if (bs_drv5055_read_mv(&mv) == 0) { bs_putfix(mv, 1000); bs_puts("V("); bs_putfix(bs_drv5055_dmt(mv, 1650, 7500), 10); bs_puts("mT)"); } else { bs_puts("none"); errors++; }

        if (!imu_on) imu_on = (bs_bmi160_init(&imu, 4, 500, 100) == 0);
        if (imu_on) {
            bs_imu_raw_t r; int32_t tc;
            if (bs_bmi160_read(&imu, &r) == 0) {
                bs_puts(" bmi=@"); bs_puthex(imu.addr, 2);
                bs_puts(" a="); bs_puti(bs_bmi160_mg(&imu, r.ax)); bs_console_putc(','); bs_puti(bs_bmi160_mg(&imu, r.ay)); bs_console_putc(','); bs_puti(bs_bmi160_mg(&imu, r.az)); bs_puts("mg");
                bs_puts(" g="); bs_puti(bs_bmi160_mdps(&imu, r.gx)); bs_console_putc(','); bs_puti(bs_bmi160_mdps(&imu, r.gy)); bs_console_putc(','); bs_puti(bs_bmi160_mdps(&imu, r.gz)); bs_puts("mdps");
                if (bs_bmi160_temp_cc(&imu, &tc) == 0) { bs_puts(" t="); bs_putfix(tc, 100); bs_puts("C"); }
                if (!mag_on) mag_on = (bs_bmm150_init(&imu) == 0);
                int16_t mx, my, mz;
                bs_puts(" mag=");
                if (mag_on && bs_bmm150_read_raw(&imu, &mx, &my, &mz) == 0) { bs_puti(mx); bs_console_putc(','); bs_puti(my); bs_console_putc(','); bs_puti(mz); } else bs_puts("none");
            } else { imu_on = 0; mag_on = 0; bs_puts(" bmi=none"); errors++; }
        } else { bs_puts(" bmi=none"); errors++; }
        bs_puts(" err="); bs_putu(errors); bs_puts("\r\n");
        while ((uint32_t)(bs_port_micros() - t0) < 1000000u) { }
    }
}
