/*
 * cmd_parse.c - see cmd_parse.h. Fox Red Labs, 2026-10-08.
 */
#include "cmd_parse.h"

#include <stdio.h>
#include <string.h>

static int is_space(uint8_t c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static uint8_t lower(uint8_t c)
{
    return (c >= 'A' && c <= 'Z') ? (uint8_t)(c + ('a' - 'A')) : c;
}

/* Next whitespace-separated token from p[*i..len). Returns its length. */
static size_t next_token(const uint8_t *p, size_t len, size_t *i, const uint8_t **tok)
{
    while (*i < len && is_space(p[*i])) {
        (*i)++;
    }
    *tok = p + *i;
    size_t start = *i;
    while (*i < len && !is_space(p[*i]) && p[*i] != 0) {
        (*i)++;
    }
    return *i - start;
}

static int tok_is(const uint8_t *tok, size_t n, const char *word)
{
    size_t w = strlen(word);
    if (n != w) {
        return 0;
    }
    for (size_t k = 0; k < n; k++) {
        if (lower(tok[k]) != (uint8_t)word[k]) {
            return 0;
        }
    }
    return 1;
}

static int rest_is_empty(const uint8_t *p, size_t len, size_t i)
{
    const uint8_t *tok;
    return next_token(p, len, &i, &tok) == 0;
}

int cmd_token_equal(const uint8_t *a, size_t alen, const char *b)
{
    size_t blen = strlen(b);
    unsigned diff = (unsigned)(alen ^ blen);

    /* Always walk CMD_TOKEN_MAX bytes so timing does not show where they differ */
    for (size_t k = 0; k < CMD_TOKEN_MAX; k++) {
        uint8_t x = (k < alen) ? a[k] : 0;
        uint8_t y = (k < blen) ? (uint8_t)b[k] : 0;
        diff |= (unsigned)(x ^ y);
    }
    return diff == 0 && alen <= CMD_TOKEN_MAX;
}

int cmd_url_host_ok(const char *url, const char *ota_host)
{
    static const char scheme[] = "https://";
    const char *auth, *end, *colon;
    size_t hlen;

    if (url == NULL || ota_host == NULL || ota_host[0] == 0) {
        return 0;
    }
    for (size_t k = 0; k < 8; k++) {
        if (url[k] == 0 || lower((uint8_t)url[k]) != (uint8_t)scheme[k]) {
            return 0;
        }
    }
    auth = url + 8;
    end = auth + strlen(auth);
    /* No user info, no path, query or fragment, no IPv6 brackets */
    if (strpbrk(auth, "@/?#[]\\") != NULL) {
        return 0;
    }
    colon = strchr(auth, ':');
    if (colon) {
        const char *p = colon + 1;
        unsigned long port = 0;
        if (p == end || (end - p) > 5) {
            return 0;
        }
        for (; p < end; p++) {
            if (*p < '0' || *p > '9') {
                return 0;
            }
            port = port * 10 + (unsigned long)(*p - '0');
        }
        if (port == 0 || port > 65535) {
            return 0;
        }
        hlen = (size_t)(colon - auth);
    } else {
        hlen = (size_t)(end - auth);
    }
    return hlen > 0 && hlen == strlen(ota_host) && memcmp(auth, ota_host, hlen) == 0;
}

cmd_type_t cmd_parse(const uint8_t *payload, size_t len, const char *expected_token,
                     const char *ota_host, cmd_t *out)
{
    const uint8_t *tok;
    size_t i = 0, n;

    memset(out, 0, sizeof(*out));
    if (payload == NULL || len == 0) {
        return out->type = CMD_NONE;
    }
    n = next_token(payload, len, &i, &tok);
    if (n == 0) {
        return out->type = CMD_NONE;
    }

    /* Token field first: "token=<value>" */
    {
        size_t elen = expected_token ? strlen(expected_token) : 0;
        int ok = 0;
        if (elen >= CMD_TOKEN_MIN && elen <= CMD_TOKEN_MAX && n > 6 && tok_is(tok, 6, "token=")) {
            ok = cmd_token_equal(tok + 6, n - 6, expected_token);
        }
        if (!ok) {
            return out->type = CMD_UNAUTHORIZED;
        }
    }

    n = next_token(payload, len, &i, &tok);
    if (n == 0) {
        return out->type = CMD_NONE;
    }
    out->type = CMD_UNKNOWN;

    if (tok_is(tok, n, "publish")) {
        if (rest_is_empty(payload, len, i)) {
            out->type = CMD_PUBLISH;
        }
    } else if (tok_is(tok, n, "forget-wifi")) {
        if (rest_is_empty(payload, len, i)) {
            out->type = CMD_FORGET_WIFI;
        }
    } else if (tok_is(tok, n, "led")) {
        n = next_token(payload, len, &i, &tok);
        if (rest_is_empty(payload, len, i)) {
            if (tok_is(tok, n, "red")) {
                out->type = CMD_LED;
                out->led = LED_RED;
            } else if (tok_is(tok, n, "green")) {
                out->type = CMD_LED;
                out->led = LED_GREEN;
            } else if (tok_is(tok, n, "blue")) {
                out->type = CMD_LED;
                out->led = LED_BLUE;
            }
        }
    } else if (tok_is(tok, n, "ota")) {
        n = next_token(payload, len, &i, &tok);
        if (!rest_is_empty(payload, len, i)) {
            /* trailing junk: unknown */
        } else if (tok_is(tok, n, "accept")) {
            out->type = CMD_OTA_ACCEPT;
        } else if (tok_is(tok, n, "reject")) {
            out->type = CMD_OTA_REJECT;
        } else if (n > 8 && n < CMD_URL_MAX) {
            memcpy(out->url, tok, n);
            out->url[n] = 0;
            /* drop one trailing slash, as TI's example does */
            if (out->url[n - 1] == '/') {
                out->url[n - 1] = 0;
            }
            if (cmd_url_host_ok(out->url, ota_host)) {
                out->type = CMD_OTA_START;
            } else {
                memset(out->url, 0, sizeof(out->url));
            }
        }
    }
    if (out->type == CMD_UNKNOWN) {
        out->led = 0;
    }
    return out->type;
}

const char *cmd_name(cmd_type_t t)
{
    switch (t) {
    case CMD_NONE:        return "none";
    case CMD_UNAUTHORIZED: return "unauthorized";
    case CMD_PUBLISH:     return "publish";
    case CMD_LED:         return "led";
    case CMD_OTA_START:   return "ota-start";
    case CMD_OTA_ACCEPT:  return "ota-accept";
    case CMD_OTA_REJECT:  return "ota-reject";
    case CMD_FORGET_WIFI: return "forget-wifi";
    default:              return "unknown";
    }
}

int topic_join(char *out, size_t out_len, const char *prefix, const char *leaf)
{
    if (out == NULL || prefix == NULL || leaf == NULL || prefix[0] == 0) {
        return -1;
    }
    if (strpbrk(prefix, "+#") != NULL || strpbrk(leaf, "+#") != NULL) {
        return -1;
    }
    int n = snprintf(out, out_len, "%s/%s", prefix, leaf);
    if (n < 0 || (size_t)n >= out_len) {
        return -1;
    }
    return n;
}

int telemetry_json(char *out, size_t out_len, uint32_t count, uint32_t uptime_s,
                   int have_temp, int temp_c, const char *fw)
{
    char t[16];
    int n;

    if (out == NULL || fw == NULL || strpbrk(fw, "\"\\") != NULL) {
        return -1;
    }
    if (have_temp) {
        snprintf(t, sizeof(t), "%d", temp_c);
    } else {
        strcpy(t, "null");
    }
    n = snprintf(out, out_len, "{\"count\":%lu,\"uptime_s\":%lu,\"chip_temp_c\":%s,\"fw\":\"%s\"}",
                 (unsigned long)count, (unsigned long)uptime_s, t, fw);
    if (n < 0 || (size_t)n >= out_len) {
        return -1;
    }
    return n;
}
