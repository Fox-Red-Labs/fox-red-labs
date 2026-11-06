/*
 * cred_record.c - see cred_record.h. Fox Red Labs, 2026-10-08.
 */
#include "cred_record.h"

#include <string.h>

uint32_t cred_crc32(const uint8_t *p, size_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

static size_t trim_nul(const uint8_t *p, size_t n)
{
    while (n > 0 && p[n - 1] == 0) {
        n--;
    }
    return n;
}

static int has_nul(const char *p, size_t n)
{
    return memchr(p, 0, n) != NULL;
}

int cred_validate(const cred_t *c)
{
    if (c == NULL) {
        return CRED_E_ARG;
    }
    if (c->ssid_len == 0 || c->ssid_len > CRED_SSID_MAX || has_nul(c->ssid, c->ssid_len)) {
        return CRED_E_SSID;
    }
    switch (c->sec) {
    case CRED_SEC_OPEN:
        if (c->pass_len != 0) {
            return CRED_E_PASS;
        }
        break;
    case CRED_SEC_WPA2:
    case CRED_SEC_WPA3:
        if (c->pass_len < CRED_PASS_MIN || c->pass_len > CRED_PASS_MAX ||
            has_nul(c->pass, c->pass_len)) {
            return CRED_E_PASS;
        }
        break;
    default:
        return CRED_E_SEC;
    }
    return CRED_OK;
}

int cred_from_parts(cred_t *c, const uint8_t *ssid, size_t ssid_len,
                    const uint8_t *pass, size_t pass_len, int sec)
{
    if (c == NULL || (ssid == NULL && ssid_len) || (pass == NULL && pass_len)) {
        return CRED_E_ARG;
    }
    memset(c, 0, sizeof(*c));
    ssid_len = ssid ? trim_nul(ssid, ssid_len) : 0;
    pass_len = pass ? trim_nul(pass, pass_len) : 0;
    if (ssid_len == 0 || ssid_len > CRED_SSID_MAX) {
        return CRED_E_SSID;
    }
    if (pass_len > CRED_PASS_MAX) {
        return CRED_E_PASS;
    }
    if (sec < CRED_SEC_OPEN || sec > CRED_SEC_WPA3) {
        return CRED_E_SEC;
    }
    memcpy(c->ssid, ssid, ssid_len);
    c->ssid_len = (uint8_t)ssid_len;
    if (sec != CRED_SEC_OPEN) {
        if (pass_len) {
            memcpy(c->pass, pass, pass_len);
        }
        c->pass_len = (uint8_t)pass_len;
    }
    c->sec = (uint8_t)sec;
    return cred_validate(c);
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int cred_encode(const cred_t *c, uint8_t *out, size_t out_len)
{
    int rc = cred_validate(c);
    if (rc != CRED_OK) {
        return rc;
    }
    if (out == NULL || out_len < CRED_RECORD_SIZE) {
        return CRED_E_ARG;
    }
    memset(out, 0, CRED_RECORD_SIZE);
    put32(out, CRED_RECORD_MAGIC);
    out[4] = CRED_RECORD_VER;
    out[5] = c->sec;
    out[6] = c->ssid_len;
    out[7] = c->pass_len;
    memcpy(out + 8, c->ssid, c->ssid_len);
    memcpy(out + 8 + CRED_SSID_MAX, c->pass, c->pass_len);
    put32(out + CRED_RECORD_SIZE - 4, cred_crc32(out, CRED_RECORD_SIZE - 4));
    return CRED_RECORD_SIZE;
}

int cred_decode(const uint8_t *in, size_t in_len, cred_t *c)
{
    if (in == NULL || c == NULL || in_len < CRED_RECORD_SIZE) {
        return CRED_E_ARG;
    }
    memset(c, 0, sizeof(*c));
    if (get32(in) != CRED_RECORD_MAGIC) {
        return CRED_E_MAGIC;
    }
    if (in[4] != CRED_RECORD_VER) {
        return CRED_E_VERSION;
    }
    if (get32(in + CRED_RECORD_SIZE - 4) != cred_crc32(in, CRED_RECORD_SIZE - 4)) {
        return CRED_E_CRC;
    }
    c->sec = in[5];
    c->ssid_len = in[6];
    c->pass_len = in[7];
    if (c->ssid_len > CRED_SSID_MAX) {
        return CRED_E_SSID;
    }
    if (c->pass_len > CRED_PASS_MAX) {
        return CRED_E_PASS;
    }
    memcpy(c->ssid, in + 8, c->ssid_len);
    memcpy(c->pass, in + 8 + CRED_SSID_MAX, c->pass_len);
    return cred_validate(c);
}

void cred_wipe(cred_t *c)
{
    if (c) {
        volatile uint8_t *p = (volatile uint8_t *)c;
        for (size_t i = 0; i < sizeof(*c); i++) {
            p[i] = 0;
        }
    }
}
