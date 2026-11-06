/*
 * cmd_parse.h - MQTT command parser, topic builder and telemetry formatter for
 * a provision, connect, publish and update-over-the-air device.
 *
 * Board-independent: plain C, no vendor headers. The device subscribes to one
 * command topic, <prefix>/cmd. Every payload starts with a shared token field,
 * then the command (case-insensitive keywords, extra spaces allowed):
 *
 *   token=<token> publish                  publish one telemetry message now
 *   token=<token> led red|green|blue       toggle that LED
 *   token=<token> ota https://host[:port]  query that OTA server and install the
 *                                          newest vendor image it offers; host
 *                                          must equal the configured OTA host
 *   token=<token> ota accept               keep a trial image after an update
 *   token=<token> ota reject               roll a trial image back
 *   token=<token> forget-wifi              erase stored Wi-Fi credentials and
 *                                          reboot into Bluetooth provisioning
 *
 * The token is compared in constant time. A configured token shorter than
 * CMD_TOKEN_MIN characters rejects every command, so a placeholder left in the
 * config cannot be used. These are bench guards on plain MQTT, not transport
 * security: anyone who can read the broker's traffic sees the token.
 *
 * Fox Red Labs, 2026-10-08. Host test: ../test/host_test.c
 */
#ifndef CMD_PARSE_H
#define CMD_PARSE_H

#include <stddef.h>
#include <stdint.h>

#define CMD_URL_MAX   256   /* includes the NUL */
#define CMD_TOKEN_MIN  16   /* shortest token accepted in the configuration */
#define CMD_TOKEN_MAX  64

typedef enum {
    CMD_NONE = 0,       /* empty payload */
    CMD_UNKNOWN,
    CMD_UNAUTHORIZED,   /* token field missing or wrong, or no usable token configured */
    CMD_PUBLISH,
    CMD_LED,
    CMD_OTA_START,
    CMD_OTA_ACCEPT,
    CMD_OTA_REJECT,
    CMD_FORGET_WIFI
} cmd_type_t;

typedef enum { LED_RED = 0, LED_GREEN = 1, LED_BLUE = 2 } cmd_led_t;

typedef struct {
    cmd_type_t type;
    cmd_led_t  led;              /* CMD_LED */
    char       url[CMD_URL_MAX]; /* CMD_OTA_START, NUL-terminated */
} cmd_t;

/* Parse a payload (not NUL-terminated; MQTT payloads carry a length).
 * expected_token: the shared token from the configuration.
 * ota_host: the only host an "ota <url>" command may name (exact match).
 * Always fills *out. Returns out->type. No or wrong token gives CMD_UNAUTHORIZED.
 * A malformed argument (bad colour; a URL that is not https://, is too long,
 * has user info, a path, a bad port, or a host other than ota_host) gives
 * CMD_UNKNOWN. */
cmd_type_t cmd_parse(const uint8_t *payload, size_t len, const char *expected_token,
                     const char *ota_host, cmd_t *out);

/* Constant-time equality of a (length alen) and NUL-terminated b. Time depends
 * only on alen and CMD_TOKEN_MAX, not on where the bytes differ. */
int cmd_token_equal(const uint8_t *a, size_t alen, const char *b);

/* 1 if url is https://<host>[:port] with host exactly equal to ota_host, no
 * user info, no path and a port of 1 to 65535. */
int cmd_url_host_ok(const char *url, const char *ota_host);

/* Name of a command type, for logs and status messages. */
const char *cmd_name(cmd_type_t t);

/* out = prefix + "/" + leaf. Returns the length, or -1 if it does not fit or
 * the prefix is empty or holds an MQTT wildcard (+ or #). */
int topic_join(char *out, size_t out_len, const char *prefix, const char *leaf);

/* Telemetry as one JSON object:
 *   {"count":N,"uptime_s":U,"chip_temp_c":T,"fw":"V"}
 * chip_temp_c is null when have_temp is 0. Returns the length, or -1 if it does
 * not fit. fw must not contain quote or backslash characters. */
int telemetry_json(char *out, size_t out_len, uint32_t count, uint32_t uptime_s,
                   int have_temp, int temp_c, const char *fw);

#endif /* CMD_PARSE_H */
