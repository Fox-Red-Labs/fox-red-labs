/* Small symmetric pieces for the post-quantum join demo: SHA-256, HMAC, HKDF (RFC 5869), CRC-32,
 * AES-128 and AES-128-GCM. Portable C99, no libc beyond string.h. Tested on the host against OpenSSL
 * (test/run.sh). Not constant-time hardened: a demo, not a product. */
#ifndef PQC_SYM_H
#define PQC_SYM_H
#include <stddef.h>
#include <stdint.h>

void pqc_sha256(const uint8_t *m, size_t n, uint8_t out[32]);
void pqc_hmac_sha256(const uint8_t *key, size_t kn, const uint8_t *m, size_t n, uint8_t out[32]);
/* HKDF-SHA256 with an empty salt; okm_len <= 32. */
void pqc_hkdf_sha256(const uint8_t *ikm, size_t ikm_n, const uint8_t *info, size_t info_n, uint8_t *okm, size_t okm_n);
uint32_t pqc_crc32(const uint8_t *m, size_t n);
/* AES-128-GCM, 12-byte nonce, 16-byte tag. decrypt returns 0 when the tag matches, -1 otherwise (and
 * writes nothing to out). */
void pqc_gcm_encrypt(const uint8_t key[16], const uint8_t nonce[12], const uint8_t *aad, size_t aad_n,
                     const uint8_t *in, size_t n, uint8_t *out, uint8_t tag[16]);
int pqc_gcm_decrypt(const uint8_t key[16], const uint8_t nonce[12], const uint8_t *aad, size_t aad_n,
                    const uint8_t *in, size_t n, const uint8_t tag[16], uint8_t *out);
#endif
