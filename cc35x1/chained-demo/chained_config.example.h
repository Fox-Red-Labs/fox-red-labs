/*
 * chained_config.example.h - settings for the chained demo.
 *
 * Copy to chained_config.h (git-ignored) and edit there. Never commit a real
 * network name, password or broker address. build.sh uses chained_config.h when
 * it exists and this example file otherwise.
 */
#ifndef CHAINED_CONFIG_H
#define CHAINED_CONFIG_H

/* MQTT broker: a host name or a dotted IPv4 address, plain TCP (TI's MQTT
 * interface has no TLS or broker login yet). */
#define CHAINED_BROKER_HOST       "broker.example.lan"
#define CHAINED_BROKER_PORT       1883

/* Shared command token. Every payload on <prefix>/cmd must start with
 * "token=<this value> ". Generate one with:
 *     openssl rand -hex 16
 * Tokens shorter than 16 characters are refused, so this placeholder rejects
 * every command until you replace it. On plain MQTT the token travels in clear
 * text: it stops other devices on the broker, not someone reading the traffic. */
#define CHAINED_CMD_TOKEN         "replace-me"

/* The only host an "ota https://<host>[:port]" command may name, compared
 * exactly (no user info, no path). Use the address or name of the machine that
 * runs TI's OTA server. Empty refuses every update. */
#define CHAINED_OTA_HOST          ""

/* Topics are <prefix>/telemetry, <prefix>/status and <prefix>/cmd. */
#define CHAINED_TOPIC_PREFIX      "frl/cc35x1"

/* Seconds between telemetry messages. */
#define CHAINED_PUBLISH_PERIOD_S  30

/* After an update, how long the new (trial) image may take to reach the broker
 * before it rolls itself back. Accepting is still a command ("ota accept"). */
#define CHAINED_TRIAL_TIMEOUT_S   300

/* Read the radio's temperature through the Wi-Fi driver's calibrator call
 * (Wlan_Get WLAN_GET_CALIBRATOR_TEMPERATURE). Set to 0 if it misbehaves; the
 * telemetry then reports chip_temp_c as null. */
#define CHAINED_READ_CHIP_TEMP    1

/* Optional: a network compiled in, for a bench test without the phone. Leave
 * the SSID empty to always use Bluetooth provisioning on first boot.
 * Security: CRED_SEC_OPEN, CRED_SEC_WPA2 or CRED_SEC_WPA3. */
#define CHAINED_DEFAULT_SSID      ""
#define CHAINED_DEFAULT_PASS      ""
#define CHAINED_DEFAULT_SEC       CRED_SEC_WPA2

#endif
