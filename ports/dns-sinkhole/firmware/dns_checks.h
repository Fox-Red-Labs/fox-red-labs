/* Pure checks used by dns_sinkhole.c, kept free of lwIP so tests/test_checks.c can run on the Mac. */
#ifndef DNS_CHECKS_H
#define DNS_CHECKS_H
#include <stdint.h>
#include <string.h>

#define DNS_MAX_RESP 1232   /* largest answer relayed; the usual EDNS size that avoids IP fragments */
#define DNS_MAX_QUERY 512

/* Source is on the board's own subnet. All three values in the same byte order. */
static inline int dns_same_subnet(uint32_t src, uint32_t own, uint32_t mask)
{
    return mask != 0 && ((src & mask) == (own & mask));
}

static inline uint8_t dns_lc(uint8_t c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

/* A reply is accepted only if it is a response with exactly one question that equals the stored question section
 * (name compared exactly when exact is set, else without case; then type and class), and is no larger than DNS_MAX_RESP. qs/qslen = bytes from offset 12
 * of the query up to and including class. */
static inline int dns_reply_matches(const uint8_t *r, uint16_t rlen, const uint8_t *qs, uint16_t qslen, int exact)
{
    if (rlen < 12u + qslen || rlen > DNS_MAX_RESP) return 0;
    if (!(r[2] & 0x80) || r[4] != 0 || r[5] != 1) return 0;
    uint16_t i = 0;
    while (i < qslen && qs[i]) {                       /* labels */
        uint8_t l = qs[i];
        if (r[12 + i] != l) return 0;
        for (uint8_t k = 1; k <= l; k++)
            if (i + k >= qslen || (exact ? r[12 + i + k] != qs[i + k] : dns_lc(r[12 + i + k]) != dns_lc(qs[i + k]))) return 0;
        i += l + 1;
    }
    if (i + 5 != qslen || r[12 + i] != 0) return 0;
    return memcmp(r + 12 + i + 1, qs + i + 1, 4) == 0;
}
#endif
