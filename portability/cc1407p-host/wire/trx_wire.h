/*
 * Field-by-field wire packer and unpacker for the CC1407P host protocol (TI's host driver, TRX.h).
 *
 * Why it exists: TI's driver lays C structs and bit-fields over the SPI buffer. That gives the right bytes
 * only where a byte is 8 bits and enums are 1 byte (Arm with short enums). On the C28x a char is 16 bits,
 * so every struct member takes a whole word and the stream is wrong. This module writes and reads each
 * field at an explicit octet offset, so the same octet stream comes out on every target.
 *
 * Rules that make it portable:
 *  - One "octet" is one element of a trx_octet array, holding 0..255. On a byte-addressed MCU that is one
 *    byte; on the C28x it is one 16-bit word with the top 8 bits zero. Convert to the SPI unit in the HAL.
 *  - Field inputs are uint16_t/uint32_t/int16_t, never uint8_t or a bit-field (the C28x has no 8-bit type).
 *  - All shifts widen first (1UL / (uint32_t)), because int is 16 bits on the C28x.
 *  - Nothing here uses sizeof as a byte count; lengths are the TRX_W_* constants in octets.
 * Layout source: the offsets in TRX.h comments, checked against the structs on a byte-addressed host
 * (wire/ref_gen.c, built with -fshort-enums).
 */
#ifndef TRX_WIRE_H
#define TRX_WIRE_H

#include <stdint.h>

typedef unsigned char trx_octet;   /* 8 bits used; one array element per wire octet */

/* Octet lengths of the wire structures (what TI's driver puts in a request's len field). */
#define TRX_W_TXN_HDR            4u
#define TRX_W_TXN_CRC            2u
#define TRX_W_REQ_HDR            2u
#define TRX_W_CONFIG_LIST        2u
#define TRX_W_NV_ERASE           2u
#define TRX_W_PING               3u
#define TRX_W_DEVCFG_STORE       14u
#define TRX_W_CONFIG_STORE_HDR   8u    /* plus the data octets */
#define TRX_W_CONFIG_PERSIST     3u
#define TRX_W_CMD_STORE          48u   /* fixed: 20 + a 27-octet parameter union, rounded up to 4 */
#define TRX_W_CMD_SUBMIT         3u
#define TRX_W_CMD_STOP           4u
#define TRX_W_STREAM_STORE_HDR   4u    /* plus the data octets */
#define TRX_W_PAYLOAD_HDR_SUNFSK 4u
#define TRX_W_LAST_STATUS_HDR    4u    /* plus params */
#define TRX_W_CMD_STATUS_HDR     8u    /* plus params */
#define TRX_W_STREAM_DESC        8u

/* Request type codes (Req_* in TRX.h). */
#define TRX_REQ_LAST_STATUS      1u
#define TRX_REQ_TRANSPORT_ERROR  2u
#define TRX_REQ_CMD_STORE        16u
#define TRX_REQ_CMD_SUBMIT       17u
#define TRX_REQ_CMD_STOP         18u
#define TRX_REQ_CMD_STATUS       19u
#define TRX_REQ_STREAM_STORE     32u
#define TRX_REQ_CONFIG_STORE     48u
#define TRX_REQ_CONFIG_PERSIST   49u
#define TRX_REQ_CONFIG_LIST      50u
#define TRX_REQ_CONFIG_LIST_RSP  51u
#define TRX_REQ_DEVCFG_STORE     56u
#define TRX_REQ_NV_ERASE         86u
#define TRX_REQ_PING             127u

/* ---- Octet helpers ---- */
void     trx_w_put16(trx_octet *p, uint16_t v);          /* little endian */
void     trx_w_put32(trx_octet *p, uint32_t v);          /* little endian */
uint16_t trx_w_get16(const trx_octet *p);
uint32_t trx_w_get32(const trx_octet *p);
/* Serialise n 32-bit words (a radio settings table) as 4n octets, little endian. Returns 4n. */
unsigned trx_w_words_to_octets(trx_octet *out, const uint32_t *words, unsigned n);
/* Octets one 16-bit-char target needs to send n octets over a byte SPI is n units: no packing needed. */

/* CRC-16 (poly 0x1021, init 0xFFFF, no reflection) over n octets; same value as TI's Plat_Util_computeCRC. */
uint16_t trx_w_crc16(const trx_octet *p, unsigned n);

/* ---- Transaction (host to radio) ---- */
/* Header only: strobe, len, tid_tx (low nibble), tid_ack (high nibble), strobe_param. */
void trx_w_txn_header(trx_octet *p, uint16_t strobe, uint16_t len, uint16_t tid_tx, uint16_t tid_ack, uint16_t strobe_param);
/* Append request bytes after the header: pads the running payload length to 4, returns the new length. */
unsigned trx_w_pad4(trx_octet *payload, unsigned len);
/* Finish a transaction already holding the 4 header octets and `payload_len` payload octets (padded as the
 * driver does): writes len, tid_tx, tid_ack, then the CRC (big endian) after the payload. Returns total octets. */
unsigned trx_w_txn_finish(trx_octet *txn, unsigned payload_len, uint16_t tid_tx, uint16_t tid_ack);

/* ---- Requests: each writes at p and returns its length in octets (not padded) ---- */
unsigned trx_w_req_config_list(trx_octet *p);
unsigned trx_w_req_nv_erase(trx_octet *p);
unsigned trx_w_req_ping(trx_octet *p, uint16_t ping_data);

typedef struct {
    uint16_t dio[7];        /* dio0, dio2, dio3, dio4, dio5, dio6, dio7 codes (0..255) */
    uint16_t clock_config;  /* 0 RC, 1 crystal, 2 TCXO */
    uint16_t dwell_time_us;
    uint16_t power_policy;  /* 0 disallow, 1 allow standby */
    uint16_t rf_mode;
} trx_w_devcfg;
unsigned trx_w_req_devcfg_store(trx_octet *p, const trx_w_devcfg *c);

/* start/md are 0 or 1; data is already octets (use trx_w_words_to_octets for a settings table). */
unsigned trx_w_req_config_store(trx_octet *p, uint16_t config_id, uint16_t start, uint16_t md, uint32_t reference,
                                const trx_octet *data, unsigned data_len);
unsigned trx_w_req_config_persist(trx_octet *p, uint16_t config_id);

typedef struct { uint16_t option_mask; uint16_t config_id; } trx_w_phy;   /* config_id 0..15 */
typedef struct {
    uint16_t slot, cmd_id;
    uint16_t slot_on_true, enable_on_true, slot_on_false, enable_on_false, slot_on_compare, enable_on_compare;
    int16_t  next_status_compare;
    uint16_t trigger, conflict_policy, allow_delay, chain_trigger, chain_conflict_policy, chain_allow_delay;
    uint32_t trigger_param, chain_trig_param;
    /* transmit parameters (TRX_Request_CommandStore_Transmit_Params) */
    trx_w_phy phy[3];
    uint16_t stream_id;
    uint32_t frequency_khz; /* 24 bits */
    uint16_t pa, modem;
    uint16_t power;         /* TRX_TxPower raw octet: fraction in bit 0, dBm in bits 1..7 */
} trx_w_cmd_tx;
unsigned trx_w_req_cmd_store_tx(trx_octet *p, const trx_w_cmd_tx *c);
unsigned trx_w_req_cmd_submit(trx_octet *p, uint16_t slot);
unsigned trx_w_req_cmd_stop(trx_octet *p, uint16_t slot, uint16_t stop_type);
/* start/md 0 or 1; stream_id 0..63. Data follows the 4-octet header. */
unsigned trx_w_req_stream_store(trx_octet *p, uint16_t stream_id, uint16_t start, uint16_t md, uint16_t retention,
                                const trx_octet *data, unsigned data_len);
/* The 4-octet SUN FSK payload header at the front of a transmitted packet. */
unsigned trx_w_payload_hdr_sunfsk(trx_octet *p, uint16_t length, uint16_t whitening, uint16_t fcs_mode,
                                  uint16_t mode_switch, uint16_t modulation, uint16_t new_phy_id);
uint16_t trx_w_txpower(uint16_t fraction, int16_t dbm);

/* ---- Radio to host: unpackers read from octets ---- */
typedef struct {
    uint16_t free_blocks, error, data_rdy_n, chip_rdy_n, len, tid_tx, tid_ack, status;
} trx_w_rx_header;
void trx_w_rx_header_parse(const trx_octet *p, trx_w_rx_header *h);
/* 1 when the CRC (big endian, after header and len payload octets) matches. */
int  trx_w_rx_crc_ok(const trx_octet *txn, unsigned payload_len);

typedef struct { uint16_t len, request; } trx_w_req_header;
void trx_w_req_header_parse(const trx_octet *p, trx_w_req_header *h);

typedef struct { uint16_t req, strobe, tid, status; unsigned params_len; } trx_w_last_status;
void trx_w_last_status_parse(const trx_octet *p, trx_w_last_status *s);

typedef struct { uint16_t status, command_id, slot, chain_action, chain_target; unsigned params_len; } trx_w_cmd_status;
void trx_w_cmd_status_parse(const trx_octet *p, trx_w_cmd_status *s);

typedef struct { uint16_t stream_id, persisted, length; uint32_t reference; } trx_w_stream_desc;
void trx_w_stream_desc_parse(const trx_octet *p, trx_w_stream_desc *d);

#endif
