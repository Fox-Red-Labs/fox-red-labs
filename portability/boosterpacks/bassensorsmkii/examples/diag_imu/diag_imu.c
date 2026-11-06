/* diag_imu: why does the BMI160 not answer? Three probes, repeated every two seconds:
 *  1. read-only scan (a 1 byte read at each address, instead of the address-only write the normal scan uses)
 *  2. BMI160 soft reset then chip ID at 0x68 and 0x69
 *  3. the same with the other three sensors powered off (bus loading) */
#include "bassens.h"
#include "bs_board.h"
#include "bs_print.h"

static void rscan(void)
{
    bs_puts(" rscan=");
    int first = 1;
    for (uint8_t a = 0x08; a < 0x78; a++) { uint8_t b; if (bs_port_i2c_read(a, &b, 1) == 0) { if (!first) bs_console_putc(','); bs_puthex(a, 2); first = 0; } }
    if (first) bs_puts("none");
}
static void id(void)
{
    for (uint8_t a = 0x68; a <= 0x69; a++) {
        uint8_t v = 0xAA;
        int r1 = bs_reg_write(a, 0x7E, 0xB6);
        bs_port_delay_ms(30);
        int r2 = bs_reg_read(a, 0x00, &v, 1);
        bs_puts(" @"); bs_puthex(a, 2); bs_puts(" rst="); bs_puti(r1); bs_puts(" id="); bs_puti(r2); bs_console_putc('/'); bs_puthex(v, 2);
    }
}
int main(void)
{
    bs_board_init();
    bs_puts("\r\ndiag_imu\r\n");
    for (uint32_t n = 1;; n++) {
        bs_puts("n="); bs_putu(n);
        bs_puts(" all-on"); rscan(); id();
        bs_power(BS_PIN_HDC_PWR, 0); bs_power(BS_PIN_OPT_PWR, 0); bs_power(BS_PIN_TMP_PWR, 0); bs_power(BS_PIN_DRV_PWR, 0);
        bs_port_delay_ms(100);
        bs_puts(" | others-off"); rscan(); id();
        bs_power_all(1);
        bs_puts("\r\n");
        bs_port_delay_ms(2000);
    }
}
