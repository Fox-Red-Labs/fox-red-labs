/* diag_imu2: BMI160 at 0x68 and 0x69, soft reset then a 100 ms wait before the chip ID read, 100 kHz bus.
 * One short line a second:  n=1 68:w=-1,id=-1 69:w=-1,id=-1 hdc=0  (w = reset write status, id = chip ID read status; the BMI160 ID is 0xD1)
 * Also reads the BMI160 ID with no reset first, and the HDC2080 ID (0x07D0 at 0xFE) as the bus-alive check. */
#include "bassens.h"
#include "bs_board.h"
#include "bs_print.h"

int main(void)
{
    bs_board_init();
    bs_puts("\r\ndiag_imu2\r\n");
    for (uint32_t n = 1;; n++) {
        bs_puts("n="); bs_putu(n);
        for (uint8_t a = 0x68; a <= 0x69; a++) {
            uint8_t v = 0xAA, v2 = 0xAA;
            int r0 = bs_reg_read(a, 0x00, &v, 1);
            int w = bs_reg_write(a, 0x7E, 0xB6);
            bs_port_delay_ms(100);
            int r1 = bs_reg_read(a, 0x00, &v2, 1);
            bs_puts(" "); bs_puthex(a, 2); bs_puts(":pre="); bs_puti(r0); bs_puts("/"); bs_puthex(v, 2);
            bs_puts(",w="); bs_puti(w); bs_puts(",id="); bs_puti(r1); bs_puts("/"); bs_puthex(v2, 2);
        }
        uint8_t h[2] = {0, 0};
        int rh = bs_reg_read(BS_ADDR_HDC2080, 0xFE, h, 2);
        bs_puts(" hdcid="); bs_puti(rh); bs_puts("/"); bs_puthex(h[1], 2); bs_puthex(h[0], 2); bs_puts("\r\n");
        bs_port_delay_ms(500);
    }
}
