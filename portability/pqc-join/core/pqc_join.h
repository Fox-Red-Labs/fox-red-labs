/* Post-quantum secure join, board side. Board-independent: a board supplies a pqc_hal_t.
 *
 * Wire format (ASCII lines, \n or \r\n ends; board lines that do not start with '@' are for people):
 *   @<TAG> <byte count> <hex> *<CRC-32 of the bytes, 8 hex digits>
 *   board -> Mac  @PK   1184  ML-KEM-768 encapsulation key
 *   Mac   -> board @CT  1088  ML-KEM-768 ciphertext
 *   board -> Mac  @KEY     4  key check: first 4 bytes of SHA-256 of the derived AES-128 key
 *   Mac   -> board @MSG  n+28 nonce(12) || AES-128-GCM ciphertext(n) || tag(16)
 *   board -> Mac  @DONE    1  0x01 decrypted and the tag matched, 0x00 it did not
 * Key: HKDF-SHA256(secret, empty salt, info "foxredlabs pqc-join v1 aes-128-gcm") -> 16 bytes.
 * GCM additional data: "pqc-join-v1". */
#ifndef PQC_JOIN_H
#define PQC_JOIN_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t (*now)(void);                                   /* CPU cycles, 64 bit */
    uint32_t cpu_hz;
    void (*rand)(uint8_t *out, size_t n);                    /* true random bytes */
    void (*puts_)(const char *s);                            /* write at once, no line end added */
    int (*getline)(char *buf, size_t max, uint32_t ms);      /* length of a received line, -1 on timeout */
    void (*stack_paint)(void);                               /* fill free stack with a pattern */
    uint32_t (*stack_used)(void);                            /* bytes of stack touched since the paint */
    void (*result)(const char *test, uint64_t cycles, int ok, const char *note); /* one measured row */
} pqc_hal_t;

#define PQC_PK_BYTES 1184
#define PQC_SK_BYTES 2400
#define PQC_CT_BYTES 1088
#define PQC_SS_BYTES 32

/* Known-answer tests (NIST ACVP ML-KEM-768 key generation test 26, first encapsulation test, first
 * decapsulation test, plus a one-bit-flip ciphertext). Returns 1 on pass. Prints and records rows. */
int pqc_kat(const pqc_hal_t *h);
/* Time keygen, encaps and decaps three times each, painting the stack before each call. Rows:
 * mlkem768_keygen|encaps|decaps (median cycles) and stack_keygen|encaps|decaps (net bytes). */
void pqc_timing(const pqc_hal_t *h);
/* Run the join once: key pair, send PK, wait for CT, decapsulate, send key check, wait for MSG,
 * decrypt, print it. Returns 1 when a message decrypted. Waits forever for the Mac. */
int pqc_join(const pqc_hal_t *h);
/* Used by the host test and by the Mac side tests. */
void pqc_hex(char *dst, const uint8_t *src, size_t n);
#endif
