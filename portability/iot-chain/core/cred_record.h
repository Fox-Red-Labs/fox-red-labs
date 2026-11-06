/*
 * cred_record.h - Wi-Fi credential record for a provision-once device.
 *
 * Board-independent: no vendor headers. A board stores the encoded record
 * wherever it keeps settings (on the CC35X1, an NVOCMP item) and decodes it at
 * boot. The record carries a magic, a version and a CRC-32 so a blank, erased or
 * half-written store reads as "no credentials" instead of garbage.
 *
 * Fox Red Labs, 2026-10-08. Host test: ../test/host_test.c
 */
#ifndef CRED_RECORD_H
#define CRED_RECORD_H

#include <stddef.h>
#include <stdint.h>

#define CRED_SSID_MAX       32   /* 802.11 SSID limit, bytes */
#define CRED_PASS_MIN        8   /* WPA2/WPA3 passphrase limits */
#define CRED_PASS_MAX       64   /* 63 characters, or 64 hex digits */

/* Encoded size: magic 4, version 1, security 1, ssid_len 1, pass_len 1,
 * ssid 32, pass 64, crc32 4. */
#define CRED_RECORD_SIZE   108
#define CRED_RECORD_MAGIC  0x46524331u   /* "FRC1" */
#define CRED_RECORD_VER      1

/* Vendor-neutral security types. Board glue maps these to its own codes. */
typedef enum {
    CRED_SEC_OPEN = 0,
    CRED_SEC_WPA2 = 1,
    CRED_SEC_WPA3 = 2
} cred_sec_t;

typedef struct {
    char    ssid[CRED_SSID_MAX + 1];   /* NUL-terminated */
    uint8_t ssid_len;
    char    pass[CRED_PASS_MAX + 1];   /* NUL-terminated, empty when open */
    uint8_t pass_len;
    uint8_t sec;                       /* cred_sec_t */
} cred_t;

/* Error codes (negative). */
#define CRED_OK             0
#define CRED_E_ARG         -1
#define CRED_E_SSID        -2
#define CRED_E_PASS        -3
#define CRED_E_SEC         -4
#define CRED_E_MAGIC       -5
#define CRED_E_VERSION     -6
#define CRED_E_CRC         -7

/* Build a record from raw bytes as a phone sends them. Trailing NUL bytes in
 * ssid or pass are dropped (some apps send them). Returns CRED_OK or an error. */
int cred_from_parts(cred_t *c, const uint8_t *ssid, size_t ssid_len,
                    const uint8_t *pass, size_t pass_len, int sec);

/* Check lengths and security type. Returns CRED_OK or an error. */
int cred_validate(const cred_t *c);

/* Encode into exactly CRED_RECORD_SIZE bytes. Returns CRED_RECORD_SIZE or an
 * error (the record is validated first). */
int cred_encode(const cred_t *c, uint8_t *out, size_t out_len);

/* Decode and validate. Returns CRED_OK or an error. */
int cred_decode(const uint8_t *in, size_t in_len, cred_t *c);

/* CRC-32 (IEEE 802.3, reflected, init and xorout 0xFFFFFFFF). */
uint32_t cred_crc32(const uint8_t *p, size_t n);

/* Overwrite a record's secrets in RAM. */
void cred_wipe(cred_t *c);

#endif /* CRED_RECORD_H */
