/* Host test for the shared part. cc -std=c99 -I. test_app.c app.c && ./a.out
 * Fakes the five glue calls, runs six steps, checks every line byte for byte. */
#include <stdio.h>
#include <string.h>
#include "app.h"

static const uint16_t samples[] = {100, 200, 300, 400, 4095, 0};
static int k, toggles;
static char got[512];
static size_t used;

void board_init(void) {}
uint16_t adc_read(void) { return samples[k++]; }
void uart_write(const char *s, uint32_t len) { memcpy(got + used, s, len); used += len; }
void led_toggle(void) { toggles++; }
void sleep_ms(uint32_t ms) { (void)ms; }

int main(void)
{
    const char *want =
        "t=0 adc=100 avg=100\r\n"
        "t=1 adc=200 avg=150\r\n"
        "t=2 adc=300 avg=200\r\n"
        "t=3 adc=400 avg=250\r\n"
        "t=4 adc=4095 avg=1248\r\n"
        "t=5 adc=0 avg=1198\r\n";
    char big[40];
    int fails = 0;
    for (uint32_t t = 0; t < 6; t++) app_step(t);
    got[used] = '\0';
    if (strcmp(got, want) != 0) { printf("FAIL lines:\n%s", got); fails++; }
    if (toggles != 6) { printf("FAIL toggles %d\n", toggles); fails++; }
    /* Longest line: t at 2^32-1, both values at 65535. Must fit the 40-byte buffer in app_step. */
    uint32_t n = app_format(big, 4294967295u, 65535, 65535);
    if (n != 34 || memcmp(big, "t=4294967295 adc=65535 avg=65535\r\n", n) != 0) { printf("FAIL long line %u\n", n); fails++; }
    printf("%s: 3 checks, %d failed\n", fails ? "FAIL" : "PASS", fails);
    return fails;
}
