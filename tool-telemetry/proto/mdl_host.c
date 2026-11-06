/* Host harness for tt_mdl.c, driven by test_mdl.py over stdin:
 *   W <hex>                          one GATT write; prints its return value
 *   C dur10 ravg rmax iavg ipk vmin tc   prints the class (-1: slot not live)
 *   S                                prints the 8-byte status as hex
 *   V                                prints the version the tag would advertise
 *   F <k>                            finishes slot k with the test key (the MAC check); prints the result
 *   K <addr hex, 12> <last counter> <command hex, 36>   checks a signed command; prints "result cmd duty counter" */
#include <stdio.h>
#include <string.h>
#include "tt_mdl.h"

static const uint8_t KEY[16] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };

int main(void)
{
    static tt_mdl_store_t s;
    char line[512];
    setvbuf(stdout, NULL, _IOLBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == 'W') {
            uint8_t w[64];
            unsigned n = 0, b;
            const char *p = line + 2;
            while (n < sizeof(w) && sscanf(p, "%2x", &b) == 1) { w[n++] = (uint8_t)b; p += 2; }
            printf("%d\n", tt_mdl_write(&s, w, (uint16_t)n));
        } else if (line[0] == 'C') {
            tt_pull_t q;
            unsigned a, b, c, d, e, f;
            int t;
            memset(&q, 0, sizeof(q));
            sscanf(line + 2, "%u %u %u %u %u %u %d", &a, &b, &c, &d, &e, &f, &t);
            q.dur10 = (uint16_t)a; q.ravg = (uint16_t)b; q.rmax = (uint16_t)c; q.iavg = (uint16_t)d; q.ipk = (uint16_t)e;
            q.vmin = (uint16_t)f; q.tc = (int8_t)t;
            printf("%d\n", tt_mdl_classify(&s, &q));
        } else if (line[0] == 'S') {
            uint8_t o[8];
            int i;
            tt_mdl_status(&s, o);
            for (i = 0; i < 8; i++) printf("%02x", o[i]);
            printf("\n");
        } else if (line[0] == 'F') {
            printf("%d\n", tt_mdl_finish(&s, (uint8_t)(line[2] - '0'), KEY));
        } else if (line[0] == 'K') {
            char ah[13], ch[37];
            unsigned long last;
            uint8_t a[6], w[TT_CMD_LEN], cmd = 0, duty = 0;
            uint32_t ctr = 0;
            unsigned b;
            int i, r;
            sscanf(line + 2, "%12s %lu %36s", ah, &last, ch);
            for (i = 0; i < 6; i++) { sscanf(ah + 2 * i, "%2x", &b); a[i] = (uint8_t)b; }
            for (i = 0; i < TT_CMD_LEN; i++) { sscanf(ch + 2 * i, "%2x", &b); w[i] = (uint8_t)b; }
            r = tt_cmd_check(KEY, a, w, (uint32_t)last, &cmd, &duty, &ctr);
            printf("%d %u %u %lu\n", r, cmd, duty, (unsigned long)ctr);
        } else if (line[0] == 'V') {
            printf("%u\n", tt_mdl_version(&s));
        }
    }
    return 0;
}
