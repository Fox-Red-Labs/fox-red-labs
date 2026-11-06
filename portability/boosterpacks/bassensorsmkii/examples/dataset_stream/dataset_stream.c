/* dataset_stream: the BMI160 at 100 Hz in the dataset-stream v1 line format that tools/dataset/capture.py records.
 *   # dataset-stream v1 rate_hz=100 sensor=bmi160
 *   t_ms,ax,ay,az,gx,gy,gz
 *   0,12,-40,16390,3,-2,1
 * Raw counts at +-2 g (16384 per g) and +-500 deg/s. t_ms is the time the sample was due (10 ms steps), so a late loop
 * shows as a gap of whole periods and a "# overrun <n>" line. -DBS_STREAM_STANDIN reads an HDC2080 register burst instead
 * of the BMI160 (sensor=standin in the header): it exercises the pipeline and is not motion data.
 * The BMI160 is retried every second, so the stream starts when the sensor answers. */
#include "bassens.h"
#include "bs_board.h"
#include "bs_print.h"

#define PERIOD_US 10000u

static bs_bmi160_t imu;

#ifdef BS_STREAM_STANDIN
#define SENSOR "standin"
static int imu_init(void) { return 0; }
static int imu_read(bs_imu_raw_t *r)
{
    uint8_t b[15];
    if (bs_reg_read(BS_ADDR_HDC2080, 0x00, b, 15)) return -1;
    r->gx = (int16_t)(b[1] << 8 | b[0]);  r->gy = (int16_t)(b[3] << 8 | b[2]);   r->gz = (int16_t)(b[5] << 8 | b[4]);
    r->ax = (int16_t)(b[7] << 8 | b[6]);  r->ay = (int16_t)(b[9] << 8 | b[8]);   r->az = (int16_t)(b[11] << 8 | b[10]);
    return 0;
}
#else
#define SENSOR "bmi160"
static int imu_init(void) { return bs_bmi160_init(&imu, 2, 500, 100); }
static int imu_read(bs_imu_raw_t *r) { return bs_bmi160_read(&imu, r); }
#endif

static void comma(void) { bs_console_putc(','); }

int main(void)
{
    bs_board_init();
    bs_puts("\r\n# dataset-stream board file: LP-MSPM0G5187 + BP-BASSENSORSMKII\r\n");
    int rc;
    while ((rc = imu_init()) != 0) {
        bs_puts("# "); bs_puts(SENSOR); bs_puts(" init failed "); bs_puti(rc); bs_puts("\r\n");
        bs_port_delay_ms(1000);
    }
    bs_puts("# dataset-stream v1 rate_hz=100 sensor=" SENSOR "\r\n");
    bs_puts("t_ms,ax,ay,az,gx,gy,gz\r\n");
    uint32_t start = bs_port_micros(), n = 0, bad = 0;
    for (;;) {
        uint32_t due = start + n * PERIOD_US;
        while ((int32_t)(bs_port_micros() - due) < 0) { }
        uint32_t late = (bs_port_micros() - due) / PERIOD_US;     /* whole periods missed */
        if (late) { bs_puts("# overrun "); bs_putu(late); bs_puts("\r\n"); n += late; due += late * PERIOD_US; }
        bs_imu_raw_t r;
        if (imu_read(&r)) { if (++bad % 100 == 1) bs_puts("# read failed\r\n"); n++; continue; }
        bs_putu(n * (PERIOD_US / 1000u)); comma();
        bs_puti(r.ax); comma(); bs_puti(r.ay); comma(); bs_puti(r.az); comma();
        bs_puti(r.gx); comma(); bs_puti(r.gy); comma(); bs_puti(r.gz); bs_puts("\r\n");
        n++;
    }
}
