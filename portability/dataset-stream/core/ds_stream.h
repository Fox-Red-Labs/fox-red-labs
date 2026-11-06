/* dataset-stream: a fixed-rate sampler that prints one CSV line per sample.
 *
 * Board-independent. The board supplies a free-running microsecond counter, a UART write
 * and the sensor read (see ../boards/README.md). Nothing here calls printf, malloc or floats,
 * so it fits a 4 kB RAM part.
 *
 * Line protocol, read by tools/dataset/capture.py on the Mac:
 *   # dataset-stream v1 rate_hz=100 sensor=bmi160          comment lines start with '#'
 *   t_ms,ax,ay,az,gx,gy,gz                                  one header line of names
 *   0,12,-40,16390,3,-2,1                                   one line per sample
 *   # overrun 3                                             samples skipped because the loop was late
 *
 * t_ms is the scheduled time of the sample, not the time the line was printed, so it sits on
 * a fixed grid (10, 20, 30 ms at 100 Hz). A skipped sample shows as a gap of whole periods.
 */
#ifndef DS_STREAM_H_
#define DS_STREAM_H_

#include <stddef.h>
#include <stdint.h>

#define DS_MAX_CHANNELS 8

typedef struct {
    uint32_t period_us;   /* 1,000,000 / rate_hz, rounded down */
    uint32_t start_us;    /* counter value at ds_init */
    uint32_t next_us;     /* counter value the next sample is due (wraps with the counter) */
    uint32_t taken;       /* samples taken */
    uint32_t overruns;    /* samples skipped because ds_due was called too late */
    uint32_t rate_hz;
} ds_sampler;

/* Start the grid at now_us. rate_hz must be 1 to 1,000,000. Returns 0, or -1 for a bad rate. */
int ds_init(ds_sampler *s, uint32_t rate_hz, uint32_t now_us);

/* Call as often as you like. Returns 1 when a sample is due and writes its scheduled time
 * in ms since ds_init to *t_ms; returns 0 otherwise. If the caller was late by one or more
 * whole periods, those samples are skipped (counted in s->overruns) and the grid is kept:
 * no burst of catch-up samples. Safe across the 32-bit counter wrap (about 71.6 minutes). */
int ds_due(ds_sampler *s, uint32_t now_us, uint32_t *t_ms);

/* "# dataset-stream v1 rate_hz=<r> sensor=<name>\n" then "t_ms,<n0>,<n1>,...\n".
 * Returns the bytes written (no terminating zero counted), or 0 if cap is too small. */
size_t ds_format_header(char *buf, size_t cap, const ds_sampler *s, const char *sensor,
                        const char *const *names, unsigned nch);

/* "<t_ms>,<v0>,<v1>,...\n". Returns the bytes written, or 0 if cap is too small or nch is 0
 * or above DS_MAX_CHANNELS. The longest line is 10 + nch * 7 bytes (t up to 4294967295,
 * each value up to -32768 with its comma, plus the newline). */
size_t ds_format_line(char *buf, size_t cap, uint32_t t_ms, const int16_t *v, unsigned nch);

/* "# overrun <n>\n". Returns the bytes written, or 0 if cap is too small. */
size_t ds_format_overrun(char *buf, size_t cap, uint32_t n);

/* Highest whole sample rate a UART can carry for lines of line_len bytes, with 8N1 framing
 * (10 bits on the wire per byte). Example: 115200 baud, 40-byte lines: 288 Hz. */
uint32_t ds_max_rate_hz(uint32_t baud, size_t line_len);

#endif
