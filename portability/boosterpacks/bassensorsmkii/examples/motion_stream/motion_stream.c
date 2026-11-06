/* motion_stream: the BMI160 sampled at 100 Hz and printed as CSV on the backchannel UART (115200 8N1), for a gesture dataset.
 * Header line, then one line per sample:  seq,host_us,sensor_ticks,ax,ay,az,gx,gy,gz
 * ax..az and gx..gz are raw counts at +-4 g (8192 per g) and +-500 deg/s (65.6 per deg/s). sensor_ticks is the BMI160's own
 * 24-bit time stamp (39.0625 us per tick), so the sample spacing is known even if the host reads late. The host reads the
 * burst every 10 ms from a free-running microsecond clock. A line "#" starts a comment (start, errors). */
#include "bassens.h"
#include "bs_board.h"
#include "bs_print.h"

#define PERIOD_US 10000u

/* -DBS_STREAM_STANDIN: no motion sensor needed. The same 15-byte burst is read from the HDC2080's registers 0x00 to 0x0E and
 * printed in the BMI160's column layout, to prove the 100 Hz host loop and the UART rate. The values are not motion data. */
#ifdef BS_STREAM_STANDIN
static int standin_read(bs_imu_raw_t *r)
{
    uint8_t b[15];
    if (bs_reg_read(BS_ADDR_HDC2080, 0x00, b, 15)) return -1;
    r->gx = (int16_t)(b[1] << 8 | b[0]);  r->gy = (int16_t)(b[3] << 8 | b[2]);   r->gz = (int16_t)(b[5] << 8 | b[4]);
    r->ax = (int16_t)(b[7] << 8 | b[6]);  r->ay = (int16_t)(b[9] << 8 | b[8]);   r->az = (int16_t)(b[11] << 8 | b[10]);
    r->t = (uint32_t)b[12] | (uint32_t)b[13] << 8 | (uint32_t)b[14] << 16;
    return 0;
}
#define IMU_INIT()   0
#define IMU_READ(r)  standin_read(r)
#else
#define IMU_INIT()   bs_bmi160_init(&imu, 4, 500, 100)
#define IMU_READ(r)  bs_bmi160_read(&imu, (r))
#endif

int main(void)
{
    bs_board_init();
    #ifdef BS_STREAM_STANDIN
    bs_puts("\r\n# motion_stream STAND-IN: HDC2080 register burst at 100 Hz, not motion data\r\n");
#else
    bs_puts("\r\n# motion_stream: BMI160 100 Hz, +-4 g, +-500 dps\r\n");
#endif
    bs_bmi160_t imu = {0}; (void)imu;
    int rc;
    while ((rc = IMU_INIT()) != 0) {
        bs_puts("# bmi160 init failed "); bs_puti(rc); bs_puts("\r\n");
        bs_port_delay_ms(1000);
    }
    bs_puts("seq,host_us,sensor_ticks,ax,ay,az,gx,gy,gz\r\n");
    uint32_t next = bs_port_micros() + PERIOD_US, seq = 0, bad = 0;
    for (;;) {
        while ((int32_t)(bs_port_micros() - next) < 0) { }
        next += PERIOD_US;
        bs_imu_raw_t r;
        uint32_t t = bs_port_micros();
        if (IMU_READ(&r)) { if (++bad % 100 == 1) bs_puts("# read failed\r\n"); continue; }
        bs_putu(seq++); bs_console_putc(','); bs_putu(t); bs_console_putc(','); bs_putu(r.t); bs_console_putc(',');
        bs_puti(r.ax); bs_console_putc(','); bs_puti(r.ay); bs_console_putc(','); bs_puti(r.az); bs_console_putc(',');
        bs_puti(r.gx); bs_console_putc(','); bs_puti(r.gy); bs_console_putc(','); bs_puti(r.gz); bs_puts("\r\n");
    }
}
