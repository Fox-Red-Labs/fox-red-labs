/* Host test: cc -I../firmware test_checks.c -o /tmp/test_checks && /tmp/test_checks */
#include <stdio.h>
#include "dns_checks.h"
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
int main(void)
{
    /* subnet: a /24 given as host-order numbers */
    CHECK(dns_same_subnet(0xC0A80147, 0xC0A80171, 0xFFFFFF00));
    CHECK(!dns_same_subnet(0x08080808, 0xC0A80171, 0xFFFFFF00));
    CHECK(!dns_same_subnet(0xC0A80247, 0xC0A80171, 0xFFFFFF00));
    CHECK(!dns_same_subnet(0xC0A80147, 0xC0A80171, 0));
    /* reply matching: question "Ads.Example.com" A IN */
    const uint8_t qs[] = {3,'A','d','s',7,'E','x','a','m','p','l','e',3,'c','o','m',0, 0,1, 0,1};
    uint8_t r[64] = {0x12,0x34, 0x81,0x80, 0,1, 0,1, 0,0, 0,0};
    memcpy(r + 12, qs, sizeof qs);
    uint16_t n = 12 + sizeof qs;
    CHECK(dns_reply_matches(r, n, qs, sizeof qs, 0));
    r[13] = 'X'; CHECK(dns_reply_matches(r, n, qs, sizeof qs, 0) == 0); r[13] = 'A';          /* other name */
    r[14] = 'D'; CHECK(dns_reply_matches(r, n, qs, sizeof qs, 0));                             /* case only */
    r[12 + sizeof qs - 3] = 28; CHECK(!dns_reply_matches(r, n, qs, sizeof qs, 0)); r[12 + sizeof qs - 3] = 1;  /* type */
    r[2] = 0x01; CHECK(!dns_reply_matches(r, n, qs, sizeof qs, 0)); r[2] = 0x81;               /* not a response */
    r[5] = 2; CHECK(!dns_reply_matches(r, n, qs, sizeof qs, 0)); r[5] = 1;                      /* two questions */
    r[14] = 'D'; CHECK(dns_reply_matches(r, n, qs, sizeof qs, 1) == 0); r[14] = 'd'; CHECK(dns_reply_matches(r, n, qs, sizeof qs, 1));   /* exact case */
    CHECK(!dns_reply_matches(r, 20, qs, sizeof qs, 0));                                         /* too short */
    CHECK(!dns_reply_matches(r, DNS_MAX_RESP + 1, qs, sizeof qs, 0));                           /* too big */
    printf(fails ? "%d failed\n" : "all passed\n", fails);
    return fails != 0;
}
