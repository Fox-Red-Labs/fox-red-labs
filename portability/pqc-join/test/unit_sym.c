/* Prints SHA-256, HMAC, HKDF, CRC-32 and AES-GCM results for fixed inputs so run.sh can compare them with OpenSSL and Python. */
#include <stdio.h>
#include <string.h>
#include "pqc_sym.h"
static void px(const char *n, const uint8_t *d, size_t k) { printf("%s ", n); for (size_t i = 0; i < k; i++) printf("%02x", d[i]); printf("\n"); }
int main(void)
{
    uint8_t d[32], buf[200], ct[200], tag[16], key[16], nonce[12], aad[] = "pqc-join-v1", okm[16];
    for (int i = 0; i < 200; i++) buf[i] = (uint8_t)(i * 7 + 3);
    for (int i = 0; i < 16; i++) key[i] = (uint8_t)(0xa0 + i);
    for (int i = 0; i < 12; i++) nonce[i] = (uint8_t)i;
    pqc_sha256(buf, 0, d); px("sha_0", d, 32);
    pqc_sha256(buf, 55, d); px("sha_55", d, 32);
    pqc_sha256(buf, 64, d); px("sha_64", d, 32);
    pqc_sha256(buf, 200, d); px("sha_200", d, 32);
    pqc_hmac_sha256(key, 16, buf, 100, d); px("hmac", d, 32);
    pqc_hkdf_sha256(buf, 32, (const uint8_t *)"info", 4, okm, 16); px("hkdf", okm, 16);
    printf("crc %08x\n", (unsigned)pqc_crc32(buf, 200));
    for (int n = 0; n <= 40; n += 13) {
        pqc_gcm_encrypt(key, nonce, aad, sizeof aad - 1, buf, (size_t)n, ct, tag);
        printf("gcm %d ", n); for (int i = 0; i < n; i++) printf("%02x", ct[i]); printf(" "); for (int i = 0; i < 16; i++) printf("%02x", tag[i]); printf("\n");
        uint8_t back[200];
        if (pqc_gcm_decrypt(key, nonce, aad, sizeof aad - 1, ct, (size_t)n, tag, back) != 0 || memcmp(back, buf, (size_t)n)) { printf("ROUNDTRIP FAIL\n"); return 1; }
        tag[0] ^= 1;
        if (pqc_gcm_decrypt(key, nonce, aad, sizeof aad - 1, ct, (size_t)n, tag, back) == 0) { printf("TAMPER NOT DETECTED\n"); return 1; }
    }
    return 0;
}
