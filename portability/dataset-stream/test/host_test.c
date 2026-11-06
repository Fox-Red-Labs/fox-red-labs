/* Host test for dataset-stream (no board): ./run.sh prints OK.
 * 1. The sampler keeps a fixed grid, skips late samples without a burst, survives the counter wrap.
 * 2. Lines and headers are formatted exactly, including -32768, and refuse a too-small buffer.
 * 3. The BMI160 reader writes the documented wake-up sequence and decodes bytes in the right order,
 *    against a fake I2C bus.
 * 4. A simulated 10 s run at 100 Hz with one 35 ms stall skips exactly 3 samples and leaves one
 *    40 ms gap in t_ms, which is what tools/dataset/check.py reports as a gap. */
#include <stdio.h>
#include <string.h>
#include "ds_stream.h"
#include "bmi160.h"

static int fails = 0;
#define CHECK(c, what) do { if (c) printf("ok   %s\n", what); else { printf("FAIL %s (line %d)\n", what, __LINE__); fails++; } } while (0)

/* ---- fake BMI160 on a fake I2C bus ---- */
typedef struct {
    uint8_t reg[128];
    uint8_t log[32][2];
    int nlog;
    uint32_t delayed_ms;
    int fail_reads;
} fake_bus;

static int fake_write(void *ctx, uint8_t addr, const uint8_t *d, size_t n)
{
    fake_bus *f = ctx;
    if (addr != 0x69 || n != 2) return -1;
    if (f->nlog < 32) { f->log[f->nlog][0] = d[0]; f->log[f->nlog][1] = d[1]; f->nlog++; }
    f->reg[d[0] & 0x7F] = d[1];
    return 0;
}
static int fake_read(void *ctx, uint8_t addr, uint8_t reg, uint8_t *d, size_t n)
{
    fake_bus *f = ctx;
    if (addr != 0x69 || f->fail_reads) return -1;
    for (size_t i = 0; i < n; i++) d[i] = f->reg[(reg + i) & 0x7F];
    return 0;
}
static void fake_delay(void *ctx, uint32_t ms) { ((fake_bus *)ctx)->delayed_ms += ms; }

int main(void)
{
    char buf[128];
    ds_sampler s;
    uint32_t t;

    /* 1. sampler */
    CHECK(ds_init(&s, 0, 0) == -1, "rate 0 refused");
    CHECK(ds_init(&s, 100, 5000) == 0 && s.period_us == 10000, "100 Hz gives a 10,000 us period");
    CHECK(ds_due(&s, 5000, &t) == 1 && t == 0, "first sample due at once, t_ms 0");
    CHECK(ds_due(&s, 5001, &t) == 0, "not due 1 us later");
    CHECK(ds_due(&s, 14999, &t) == 0, "not due 1 us before the next slot");
    CHECK(ds_due(&s, 15000, &t) == 1 && t == 10, "due on the next slot, t_ms 10");
    CHECK(ds_due(&s, 25300, &t) == 1 && t == 20 && s.overruns == 0, "300 us late is still slot 2, no overrun");
    CHECK(ds_due(&s, 25400, &t) == 0, "grid kept: slot 3 still at 35,000 us");
    CHECK(ds_due(&s, 61000, &t) == 1 && t == 50 && s.overruns == 2, "26 ms late: slots 3 and 4 skipped, sample is slot 5");
    CHECK(ds_due(&s, 61001, &t) == 0, "no catch-up burst after a skip");
    CHECK(ds_due(&s, 65000, &t) == 1 && t == 60, "back on the grid at slot 6");

    /* wrap: start 2 ms before the 32-bit counter rolls over */
    ds_init(&s, 1000, 0xFFFFF830u);
    int n = 0; uint32_t last = 0, ok_steps = 1;
    for (uint32_t k = 0; k < 10000; k++) {
        uint32_t now = 0xFFFFF830u + k * 100u;        /* poll every 100 us, wraps after 20 polls */
        if (ds_due(&s, now, &t)) { if (n && t != last + 1) ok_steps = 0; last = t; n++; }
    }
    CHECK(n == 1000 && ok_steps && s.overruns == 0, "1 kHz across the counter wrap: 1,000 samples, 1 ms apart");

    /* 2. formatting */
    int16_t v[6] = { 12, -40, 16390, 3, -2, 1 };
    size_t L = ds_format_line(buf, sizeof buf, 20, v, 6);
    CHECK(L == strlen("20,12,-40,16390,3,-2,1\n") && strcmp(buf, "20,12,-40,16390,3,-2,1\n") == 0, "sample line exact");
    int16_t ext[3] = { -32768, 32767, 0 };
    ds_format_line(buf, sizeof buf, 4294967295u, ext, 3);
    CHECK(strcmp(buf, "4294967295,-32768,32767,0\n") == 0, "extremes: max t_ms, -32768, 32767, 0");
    CHECK(strlen(buf) <= 10 + 3 * 7, "longest line within the documented 10 + 7 per channel bytes");
    CHECK(ds_format_line(buf, 10, 20, v, 6) == 0, "too-small buffer refused, returns 0");
    CHECK(ds_format_line(buf, sizeof buf, 0, v, 0) == 0, "zero channels refused");
    const char *names[6] = { "ax", "ay", "az", "gx", "gy", "gz" };
    ds_init(&s, 100, 0);
    ds_format_header(buf, sizeof buf, &s, "bmi160", names, 6);
    CHECK(strcmp(buf, "# dataset-stream v1 rate_hz=100 sensor=bmi160\nt_ms,ax,ay,az,gx,gy,gz\n") == 0, "header exact");
    ds_format_overrun(buf, sizeof buf, 3);
    CHECK(strcmp(buf, "# overrun 3\n") == 0, "overrun line exact");
    CHECK(ds_max_rate_hz(115200, 40) == 288, "115200 baud, 40-byte lines: 288 Hz");
    CHECK(ds_max_rate_hz(115200, 10 + 6 * 7) == 221, "115200 baud, worst-case 6-channel line (52 bytes): 221 Hz");

    /* 3. BMI160 against a fake bus */
    fake_bus f; memset(&f, 0, sizeof f);
    bmi160_bus b = { fake_write, fake_read, fake_delay, &f, 0x69 };
    uint8_t id = 0;
    CHECK(bmi160_init(&b, 1, &id) == BMI160_ERR_ID && id == 0x00, "wrong chip ID (0x00) refused");
    f.reg[0x00] = 0xD1;
    CHECK(bmi160_init(&b, 1, &id) == BMI160_OK && id == 0xD1, "chip ID 0xD1 accepted");
    const uint8_t want[6][2] = { {0x7E, 0x11}, {0x7E, 0x15}, {0x40, 0x28}, {0x41, 0x03}, {0x42, 0x28}, {0x43, 0x00} };
    CHECK(f.nlog == 6 && memcmp(f.log, want, sizeof want) == 0, "wake-up writes: accel normal, gyro normal, 100 Hz, +-2 g, 100 Hz, +-2000 deg/s");
    CHECK(f.delayed_ms >= 4 + 80, "waits at least 3.8 ms and 80 ms after the two mode commands");
    /* accel x = 16384 (1 g) at 0x12, y = -1, z = -32768; gyro x = 1, y = 256, z = -2 at 0x0C */
    const uint8_t data[12] = { 0x01,0x00, 0x00,0x01, 0xFE,0xFF,  0x00,0x40, 0xFF,0xFF, 0x00,0x80 };
    memcpy(&f.reg[0x0C], data, 12);
    int16_t out[6];
    CHECK(bmi160_read(&b, out) == BMI160_OK && out[0] == 16384 && out[1] == -1 && out[2] == -32768
          && out[3] == 1 && out[4] == 256 && out[5] == -2, "read decodes accel from 0x12 and gyro from 0x0C, low byte first");
    f.fail_reads = 1;
    CHECK(bmi160_read(&b, out) == BMI160_ERR_BUS, "bus error reported");
    memset(&f, 0, sizeof f); f.reg[0] = 0xD1;
    bmi160_init(&b, 0, NULL);
    CHECK(f.nlog == 3 && f.delayed_ms == 4, "accel only: three writes, one 4 ms wait");

    /* 4. a simulated 10 s run at 100 Hz, polled every 1 ms with one 35 ms stall at 5 s */
    ds_init(&s, 100, 0);
    int lines = 0; uint32_t first = 1, prev = 0, max_step = 0; size_t longest = 0;
    for (uint32_t now = 0; now < 10000000u; now += 1000u) {
        if (now == 5000000u) now += 35000u;
        if (ds_due(&s, now, &t)) {
            int16_t r[6] = { (int16_t)lines, 0, 16384, 0, 0, 0 };
            size_t l = ds_format_line(buf, sizeof buf, t, r, 6);
            if (l > longest) longest = l;
            if (!first && t - prev > max_step) max_step = t - prev;
            first = 0; prev = t; lines++;
        }
    }
    CHECK(s.overruns == 3 && lines == 997 && max_step == 40, "35 ms stall: 3 samples skipped, one 40 ms gap, 997 lines in 10 s");
    printf("     longest line in the run: %zu bytes; at 115200 baud that allows %u Hz\n", longest, ds_max_rate_hz(115200, longest));

    printf(fails ? "FAILED: %d\n" : "OK\n", fails);
    return fails ? 1 : 0;
}
