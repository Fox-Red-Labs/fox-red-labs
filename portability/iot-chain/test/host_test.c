/*
 * host_test.c - checks portability/iot-chain/core on the Mac (no board).
 * Run: portability/iot-chain/test/run.sh
 * Fox Red Labs, 2026-10-08.
 */
#include <stdio.h>
#include <string.h>

#include "cred_record.h"
#include "cmd_parse.h"

static int fails, checks;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define TOK  "0123456789abcdef0123456789abcdef"   /* 32 hex, like openssl rand -hex 16 */
#define HOST "192.168.1.20"

/* Parse with the right token prepended */
static cmd_type_t P(const char *s, cmd_t *c)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "token=" TOK " %s", s);
    return cmd_parse((const uint8_t *)buf, strlen(buf), TOK, HOST, c);
}

/* Parse exactly as given */
static cmd_type_t R(const char *s, const char *tok, const char *host, cmd_t *c)
{
    return cmd_parse((const uint8_t *)s, strlen(s), tok, host, c);
}

static void test_crc(void)
{
    /* Standard check value for CRC-32/ISO-HDLC. */
    CHECK(cred_crc32((const uint8_t *)"123456789", 9) == 0xCBF43926u);
}

static void test_cred(void)
{
    cred_t c, d;
    uint8_t rec[CRED_RECORD_SIZE];
    const uint8_t ssid[] = "HomeNet";
    const uint8_t pass[] = "correct horse";

    /* round trip, with the trailing NUL a phone may send */
    CHECK(cred_from_parts(&c, ssid, sizeof(ssid), pass, sizeof(pass), CRED_SEC_WPA2) == CRED_OK);
    CHECK(c.ssid_len == 7 && c.pass_len == 13);
    CHECK(cred_encode(&c, rec, sizeof(rec)) == CRED_RECORD_SIZE);
    CHECK(cred_decode(rec, sizeof(rec), &d) == CRED_OK);
    CHECK(strcmp(d.ssid, "HomeNet") == 0 && strcmp(d.pass, "correct horse") == 0 && d.sec == CRED_SEC_WPA2);

    /* any flipped bit is caught */
    for (size_t i = 0; i < CRED_RECORD_SIZE; i++) {
        uint8_t bad[CRED_RECORD_SIZE];
        memcpy(bad, rec, sizeof(bad));
        bad[i] ^= 0x10;
        if (cred_decode(bad, sizeof(bad), &d) == CRED_OK) {
            CHECK(0 && "bit flip accepted");
            break;
        }
    }
    CHECK(1);

    /* an erased store (all 0xFF) and a blank one (all 0) are "no credentials" */
    memset(rec, 0xFF, sizeof(rec));
    CHECK(cred_decode(rec, sizeof(rec), &d) == CRED_E_MAGIC);
    memset(rec, 0x00, sizeof(rec));
    CHECK(cred_decode(rec, sizeof(rec), &d) == CRED_E_MAGIC);
    CHECK(cred_decode(rec, CRED_RECORD_SIZE - 1, &d) == CRED_E_ARG);

    /* open network: password ignored */
    CHECK(cred_from_parts(&c, ssid, 7, pass, 13, CRED_SEC_OPEN) == CRED_OK);
    CHECK(c.pass_len == 0 && c.pass[0] == 0);

    /* limits */
    uint8_t s33[33], p65[65], p7[7];
    memset(s33, 'a', sizeof(s33));
    memset(p65, 'b', sizeof(p65));
    memset(p7, 'c', sizeof(p7));
    CHECK(cred_from_parts(&c, s33, 32, pass, 13, CRED_SEC_WPA2) == CRED_OK);
    CHECK(cred_from_parts(&c, s33, 33, pass, 13, CRED_SEC_WPA2) == CRED_E_SSID);
    CHECK(cred_from_parts(&c, ssid, 0, pass, 13, CRED_SEC_WPA2) == CRED_E_SSID);
    CHECK(cred_from_parts(&c, ssid, 7, p65, 64, CRED_SEC_WPA3) == CRED_OK);
    CHECK(cred_from_parts(&c, ssid, 7, p65, 65, CRED_SEC_WPA3) == CRED_E_PASS);
    CHECK(cred_from_parts(&c, ssid, 7, p7, 7, CRED_SEC_WPA2) == CRED_E_PASS);
    CHECK(cred_from_parts(&c, ssid, 7, NULL, 0, CRED_SEC_WPA2) == CRED_E_PASS);
    CHECK(cred_from_parts(&c, ssid, 7, pass, 13, 9) == CRED_E_SEC);

    /* a NUL inside the SSID is refused */
    const uint8_t mid[] = { 'a', 0, 'b' };
    CHECK(cred_from_parts(&c, mid, 3, NULL, 0, CRED_SEC_OPEN) == CRED_E_SSID);

    cred_from_parts(&c, ssid, 7, pass, 13, CRED_SEC_WPA2);
    cred_wipe(&c);
    CHECK(c.ssid_len == 0 && c.pass[0] == 0);
}

static void test_cmd(void)
{
    cmd_t c;

    CHECK(P("publish", &c) == CMD_PUBLISH);
    CHECK(P("  PUBLISH \r\n", &c) == CMD_PUBLISH);
    CHECK(P("publish now", &c) == CMD_UNKNOWN);
    CHECK(P("", &c) == CMD_NONE);
    CHECK(R("", TOK, HOST, &c) == CMD_NONE);
    CHECK(R("   ", TOK, HOST, &c) == CMD_NONE);
    CHECK(cmd_parse(NULL, 0, TOK, HOST, &c) == CMD_NONE);

    CHECK(P("led red", &c) == CMD_LED && c.led == LED_RED);
    CHECK(P("LED  Green", &c) == CMD_LED && c.led == LED_GREEN);
    CHECK(P("led blue", &c) == CMD_LED && c.led == LED_BLUE);
    CHECK(P("led purple", &c) == CMD_UNKNOWN);
    CHECK(P("led", &c) == CMD_UNKNOWN);

    CHECK(P("ota accept", &c) == CMD_OTA_ACCEPT);
    CHECK(P("ota REJECT", &c) == CMD_OTA_REJECT);
    CHECK(P("ota", &c) == CMD_UNKNOWN);

    CHECK(P("forget-wifi", &c) == CMD_FORGET_WIFI);
    CHECK(P("reboot", &c) == CMD_UNKNOWN);

    CHECK(strcmp(cmd_name(CMD_OTA_START), "ota-start") == 0);
    CHECK(strcmp(cmd_name(CMD_UNAUTHORIZED), "unauthorized") == 0);
}

static void test_token(void)
{
    cmd_t c;

    /* right token */
    CHECK(R("token=" TOK " publish", TOK, HOST, &c) == CMD_PUBLISH);
    CHECK(R("TOKEN=" TOK " forget-wifi", TOK, HOST, &c) == CMD_FORGET_WIFI);
    /* missing token field */
    CHECK(R("publish", TOK, HOST, &c) == CMD_UNAUTHORIZED);
    CHECK(R("forget-wifi", TOK, HOST, &c) == CMD_UNAUTHORIZED);
    CHECK(R("ota https://" HOST ":8443", TOK, HOST, &c) == CMD_UNAUTHORIZED);
    CHECK(R("token= publish", TOK, HOST, &c) == CMD_UNAUTHORIZED);
    CHECK(R("token=", TOK, HOST, &c) == CMD_UNAUTHORIZED);
    /* wrong token: one character off, at the start and the end, shorter, longer, other case */
    CHECK(R("token=1123456789abcdef0123456789abcdef forget-wifi", TOK, HOST, &c) == CMD_UNAUTHORIZED);
    CHECK(R("token=0123456789abcdef0123456789abcdee forget-wifi", TOK, HOST, &c) == CMD_UNAUTHORIZED);
    CHECK(R("token=0123456789abcdef0123456789abcde forget-wifi", TOK, HOST, &c) == CMD_UNAUTHORIZED);
    CHECK(R("token=0123456789abcdef0123456789abcdef0 forget-wifi", TOK, HOST, &c) == CMD_UNAUTHORIZED);
    CHECK(R("token=0123456789ABCDEF0123456789ABCDEF forget-wifi", TOK, HOST, &c) == CMD_UNAUTHORIZED);
    /* token in the wrong place */
    CHECK(R("forget-wifi token=" TOK, TOK, HOST, &c) == CMD_UNAUTHORIZED);
    /* right token but no command */
    CHECK(R("token=" TOK, TOK, HOST, &c) == CMD_NONE);
    /* a placeholder or empty configured token rejects everything, even a match */
    CHECK(R("token=replace-me publish", "replace-me", HOST, &c) == CMD_UNAUTHORIZED);
    CHECK(R("token= publish", "", HOST, &c) == CMD_UNAUTHORIZED);
    CHECK(R("publish", NULL, HOST, &c) == CMD_UNAUTHORIZED);
    /* constant-time helper */
    CHECK(cmd_token_equal((const uint8_t *)TOK, 32, TOK) == 1);
    CHECK(cmd_token_equal((const uint8_t *)TOK, 31, TOK) == 0);
    CHECK(cmd_token_equal((const uint8_t *)"x", 1, "") == 0);
}

static void test_ota_host(void)
{
    cmd_t c;

    CHECK(P("ota https://192.168.1.20:8443", &c) == CMD_OTA_START);
    CHECK(strcmp(c.url, "https://192.168.1.20:8443") == 0);
    CHECK(P("ota https://192.168.1.20:8443/", &c) == CMD_OTA_START);
    CHECK(strcmp(c.url, "https://192.168.1.20:8443") == 0);
    CHECK(P("ota https://192.168.1.20", &c) == CMD_OTA_START);
    CHECK(P("ota HTTPS://192.168.1.20:443", &c) == CMD_OTA_START);

    /* wrong scheme or empty */
    CHECK(P("ota http://192.168.1.20:8443", &c) == CMD_UNKNOWN);
    CHECK(P("ota https://", &c) == CMD_UNKNOWN);
    CHECK(P("ota https://a b", &c) == CMD_UNKNOWN);
    /* other hosts and lookalikes */
    CHECK(P("ota https://evil.com:8443", &c) == CMD_UNKNOWN && c.url[0] == 0);
    CHECK(P("ota https://192.168.1.200:8443", &c) == CMD_UNKNOWN);
    CHECK(P("ota https://192.168.1.2:8443", &c) == CMD_UNKNOWN);
    CHECK(P("ota https://192.168.1.20.evil.com:8443", &c) == CMD_UNKNOWN);
    CHECK(P("ota https://evil.com/192.168.1.20", &c) == CMD_UNKNOWN);
    CHECK(P("ota https://192.168.1.20@evil.com:8443", &c) == CMD_UNKNOWN);
    CHECK(P("ota https://evil.com@192.168.1.20:8443", &c) == CMD_UNKNOWN);
    CHECK(P("ota https://user:pw@192.168.1.20:8443", &c) == CMD_UNKNOWN);
    CHECK(P("ota https://192.168.1.20:8443/other/path", &c) == CMD_UNKNOWN);
    CHECK(P("ota https://192.168.1.20:8443?x=1", &c) == CMD_UNKNOWN);
    CHECK(P("ota https://192.168.1.20#frag", &c) == CMD_UNKNOWN);
    CHECK(P("ota https://[::1]:8443", &c) == CMD_UNKNOWN);
    /* ports */
    CHECK(P("ota https://192.168.1.20:", &c) == CMD_UNKNOWN);
    CHECK(P("ota https://192.168.1.20:0", &c) == CMD_UNKNOWN);
    CHECK(P("ota https://192.168.1.20:65535", &c) == CMD_OTA_START);
    CHECK(P("ota https://192.168.1.20:65536", &c) == CMD_UNKNOWN);
    CHECK(P("ota https://192.168.1.20:123456", &c) == CMD_UNKNOWN);
    CHECK(P("ota https://192.168.1.20:84a3", &c) == CMD_UNKNOWN);
    CHECK(P("ota https://192.168.1.20:8443:1", &c) == CMD_UNKNOWN);

    /* named host: exact match only */
    CHECK(R("token=" TOK " ota https://ota.host:8443", TOK, "ota.host", &c) == CMD_OTA_START);
    CHECK(R("token=" TOK " ota https://evil.com/ota.host", TOK, "ota.host", &c) == CMD_UNKNOWN);
    CHECK(R("token=" TOK " ota https://ota.host.evil.com", TOK, "ota.host", &c) == CMD_UNKNOWN);
    CHECK(R("token=" TOK " ota https://evilota.host", TOK, "ota.host", &c) == CMD_UNKNOWN);
    CHECK(R("token=" TOK " ota https://ota.hos", TOK, "ota.host", &c) == CMD_UNKNOWN);
    CHECK(R("token=" TOK " ota https://user@ota.host", TOK, "ota.host", &c) == CMD_UNKNOWN);
    /* no configured OTA host: every URL refused */
    CHECK(R("token=" TOK " ota https://192.168.1.20", TOK, "", &c) == CMD_UNKNOWN);
    CHECK(R("token=" TOK " ota https://192.168.1.20", TOK, NULL, &c) == CMD_UNKNOWN);

    char longurl[400] = "ota https://";
    memset(longurl + 12, 'x', 300);
    longurl[312] = 0;
    CHECK(P(longurl, &c) == CMD_UNKNOWN);
}

static void test_topic_and_json(void)
{
    char buf[64];

    CHECK(topic_join(buf, sizeof(buf), "frl/cc35x1", "cmd") == 14 && strcmp(buf, "frl/cc35x1/cmd") == 0);
    CHECK(topic_join(buf, 8, "frl/cc35x1", "cmd") == -1);
    CHECK(topic_join(buf, sizeof(buf), "", "cmd") == -1);
    CHECK(topic_join(buf, sizeof(buf), "frl/#", "cmd") == -1);

    CHECK(telemetry_json(buf, sizeof(buf), 3, 90, 1, 41, "0.0.1.0") > 0);
    CHECK(strcmp(buf, "{\"count\":3,\"uptime_s\":90,\"chip_temp_c\":41,\"fw\":\"0.0.1.0\"}") == 0);
    CHECK(telemetry_json(buf, sizeof(buf), 3, 90, 0, 0, "0.0.1.0") > 0);
    CHECK(strstr(buf, "\"chip_temp_c\":null") != NULL);
    CHECK(telemetry_json(buf, sizeof(buf), 3, 90, 1, -12, "x") > 0 && strstr(buf, ":-12,") != NULL);
    CHECK(telemetry_json(buf, 20, 3, 90, 1, 41, "0.0.1.0") == -1);
    CHECK(telemetry_json(buf, sizeof(buf), 3, 90, 1, 41, "bad\"fw") == -1);
}

int main(void)
{
    test_crc();
    test_cred();
    test_cmd();
    test_token();
    test_ota_host();
    test_topic_and_json();
    printf("%s: %d of %d checks passed\n", fails ? "FAIL" : "OK", checks - fails, checks);
    return fails ? 1 : 0;
}
