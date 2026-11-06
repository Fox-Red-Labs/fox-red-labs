/* Field-by-field wire packer for the CC1407P host protocol. See trx_wire.h. Plain C99, no bit-fields, no
 * uint8_t, no sizeof as a byte count, so it compiles and gives the same octets on the C28x and on a Mac. */
#include "trx_wire.h"

#define O(v) ((trx_octet)((v) & 0xFFu))

void trx_w_put16(trx_octet *p, uint16_t v)
{
    p[0] = O(v);
    p[1] = O((uint32_t)v >> 8);
}
void trx_w_put32(trx_octet *p, uint32_t v)
{
    p[0] = O(v);
    p[1] = O(v >> 8);
    p[2] = O(v >> 16);
    p[3] = O(v >> 24);
}
uint16_t trx_w_get16(const trx_octet *p)
{
    return (uint16_t)((uint16_t)(p[0] & 0xFFu) | (uint16_t)((p[1] & 0xFFu) << 8));
}
uint32_t trx_w_get32(const trx_octet *p)
{
    return (uint32_t)(p[0] & 0xFFu) | ((uint32_t)(p[1] & 0xFFu) << 8) | ((uint32_t)(p[2] & 0xFFu) << 16) |
           ((uint32_t)(p[3] & 0xFFu) << 24);
}
unsigned trx_w_words_to_octets(trx_octet *out, const uint32_t *words, unsigned n)
{
    unsigned i;
    for (i = 0; i < n; i++) {
        trx_w_put32(out + 4u * i, words[i]);
    }
    return 4u * n;
}

uint16_t trx_w_crc16(const trx_octet *p, unsigned n)
{
    uint16_t crc = 0xFFFFu;
    unsigned i, b;
    for (i = 0; i < n; i++) {
        crc = (uint16_t)(crc ^ (uint16_t)((p[i] & 0xFFu) << 8));
        for (b = 0; b < 8u; b++) {
            crc = (uint16_t)((crc & 0x8000u) ? ((crc << 1) ^ 0x1021u) : (crc << 1));
        }
    }
    return crc;
}

void trx_w_txn_header(trx_octet *p, uint16_t strobe, uint16_t len, uint16_t tid_tx, uint16_t tid_ack, uint16_t strobe_param)
{
    p[0] = O(strobe);
    p[1] = O(len);
    p[2] = O((tid_tx & 0x0Fu) | ((tid_ack & 0x0Fu) << 4));
    p[3] = O(strobe_param);
}
unsigned trx_w_pad4(trx_octet *payload, unsigned len)
{
    while ((len & 3u) != 0u) {
        payload[len++] = 0u;
    }
    return len;
}
unsigned trx_w_txn_finish(trx_octet *txn, unsigned payload_len, uint16_t tid_tx, uint16_t tid_ack)
{
    uint16_t crc;
    txn[1] = O(payload_len);
    txn[2] = O((tid_tx & 0x0Fu) | ((tid_ack & 0x0Fu) << 4));
    crc = trx_w_crc16(txn, TRX_W_TXN_HDR + payload_len);
    txn[TRX_W_TXN_HDR + payload_len] = O(crc >> 8);
    txn[TRX_W_TXN_HDR + payload_len + 1u] = O(crc);
    return TRX_W_TXN_HDR + payload_len + TRX_W_TXN_CRC;
}

static void req_head(trx_octet *p, unsigned len, uint16_t type)
{
    p[0] = O(len);
    p[1] = O(type & 0x7Fu);
}

unsigned trx_w_req_config_list(trx_octet *p) { req_head(p, TRX_W_CONFIG_LIST, TRX_REQ_CONFIG_LIST); return TRX_W_CONFIG_LIST; }
unsigned trx_w_req_nv_erase(trx_octet *p) { req_head(p, TRX_W_NV_ERASE, TRX_REQ_NV_ERASE); return TRX_W_NV_ERASE; }
unsigned trx_w_req_ping(trx_octet *p, uint16_t d)
{
    req_head(p, TRX_W_PING, TRX_REQ_PING);
    p[2] = O(d);
    return TRX_W_PING;
}

unsigned trx_w_req_devcfg_store(trx_octet *p, const trx_w_devcfg *c)
{
    unsigned i;
    req_head(p, TRX_W_DEVCFG_STORE, TRX_REQ_DEVCFG_STORE);
    for (i = 0; i < 7u; i++) {
        p[2u + i] = O(c->dio[i]);
    }
    p[9] = O(c->clock_config);
    trx_w_put16(p + 10, c->dwell_time_us);
    p[12] = O(c->power_policy);
    p[13] = O(c->rf_mode);
    return TRX_W_DEVCFG_STORE;
}

unsigned trx_w_req_config_store(trx_octet *p, uint16_t config_id, uint16_t start, uint16_t md, uint32_t reference,
                                const trx_octet *data, unsigned data_len)
{
    unsigned i;
    req_head(p, TRX_W_CONFIG_STORE_HDR + data_len, TRX_REQ_CONFIG_STORE);
    p[2] = O((config_id & 0x3Fu) | ((start & 1u) << 6) | ((md & 1u) << 7));
    p[3] = 0u;
    trx_w_put32(p + 4, reference);
    for (i = 0; i < data_len; i++) {
        p[TRX_W_CONFIG_STORE_HDR + i] = O(data[i]);
    }
    return TRX_W_CONFIG_STORE_HDR + data_len;
}
unsigned trx_w_req_config_persist(trx_octet *p, uint16_t config_id)
{
    req_head(p, TRX_W_CONFIG_PERSIST, TRX_REQ_CONFIG_PERSIST);
    p[2] = O(config_id & 0x3Fu);
    return TRX_W_CONFIG_PERSIST;
}

unsigned trx_w_req_cmd_store_tx(trx_octet *p, const trx_w_cmd_tx *c)
{
    unsigned i;
    uint32_t f;
    for (i = 0; i < TRX_W_CMD_STORE; i++) {
        p[i] = 0u;
    }
    req_head(p, TRX_W_CMD_STORE, TRX_REQ_CMD_STORE);
    p[2] = O(c->slot);
    p[3] = O(c->cmd_id);
    p[4] = 0u;
    p[5] = O((c->slot_on_true & 0x7Fu) | ((c->enable_on_true & 1u) << 7));
    p[6] = O((c->slot_on_false & 0x7Fu) | ((c->enable_on_false & 1u) << 7));
    p[7] = O((c->slot_on_compare & 0x3Fu) | ((c->enable_on_compare & 3u) << 6));
    trx_w_put16(p + 8, (uint16_t)c->next_status_compare);
    p[10] = O((c->trigger & 0x1Fu) | ((c->conflict_policy & 3u) << 5) | ((c->allow_delay & 1u) << 7));
    p[11] = O((c->chain_trigger & 0x1Fu) | ((c->chain_conflict_policy & 3u) << 5) | ((c->chain_allow_delay & 1u) << 7));
    trx_w_put32(p + 12, c->trigger_param);
    trx_w_put32(p + 16, c->chain_trig_param);
    /* params at 20: phy0..2 (3 octets each), stream_id, then frequency/pa/modem word, then power */
    for (i = 0; i < 3u; i++) {
        trx_w_put16(p + 20u + 3u * i, c->phy[i].option_mask);
        p[22u + 3u * i] = O(c->phy[i].config_id & 0x0Fu);
    }
    trx_w_put16(p + 29, c->stream_id);
    f = (c->frequency_khz & 0x00FFFFFFUL) | ((uint32_t)(c->pa & 1u) << 28) | ((uint32_t)(c->modem & 7u) << 29);
    trx_w_put32(p + 31, f);
    p[35] = O(c->power);
    return TRX_W_CMD_STORE;
}
unsigned trx_w_req_cmd_submit(trx_octet *p, uint16_t slot)
{
    req_head(p, TRX_W_CMD_SUBMIT, TRX_REQ_CMD_SUBMIT);
    p[2] = O(slot);
    return TRX_W_CMD_SUBMIT;
}
unsigned trx_w_req_cmd_stop(trx_octet *p, uint16_t slot, uint16_t stop_type)
{
    req_head(p, TRX_W_CMD_STOP, TRX_REQ_CMD_STOP);
    p[2] = O(slot);
    p[3] = O(stop_type);
    return TRX_W_CMD_STOP;
}
unsigned trx_w_req_stream_store(trx_octet *p, uint16_t stream_id, uint16_t start, uint16_t md, uint16_t retention,
                                const trx_octet *data, unsigned data_len)
{
    unsigned i;
    req_head(p, TRX_W_STREAM_STORE_HDR + data_len, TRX_REQ_STREAM_STORE);
    p[2] = O((stream_id & 0x3Fu) | ((start & 1u) << 6) | ((md & 1u) << 7));
    p[3] = O(retention);
    for (i = 0; i < data_len; i++) {
        p[TRX_W_STREAM_STORE_HDR + i] = O(data[i]);
    }
    return TRX_W_STREAM_STORE_HDR + data_len;
}
unsigned trx_w_payload_hdr_sunfsk(trx_octet *p, uint16_t length, uint16_t whitening, uint16_t fcs_mode,
                                  uint16_t mode_switch, uint16_t modulation, uint16_t new_phy_id)
{
    trx_w_put16(p, length);
    p[2] = O((whitening & 1u) | ((fcs_mode & 1u) << 1) | ((mode_switch & 1u) << 2) | ((modulation & 3u) << 6));
    p[3] = O(new_phy_id);
    return TRX_W_PAYLOAD_HDR_SUNFSK;
}
uint16_t trx_w_txpower(uint16_t fraction, int16_t dbm)
{
    return (uint16_t)((fraction & 1u) | (((uint16_t)dbm & 0x7Fu) << 1));
}

void trx_w_rx_header_parse(const trx_octet *p, trx_w_rx_header *h)
{
    uint16_t b0 = (uint16_t)(p[0] & 0xFFu);
    h->free_blocks = (uint16_t)(b0 & 0x0Fu);
    h->error = (uint16_t)((b0 >> 5) & 1u);
    h->data_rdy_n = (uint16_t)((b0 >> 6) & 1u);
    h->chip_rdy_n = (uint16_t)((b0 >> 7) & 1u);
    h->len = (uint16_t)(p[1] & 0xFFu);
    h->tid_tx = (uint16_t)(p[2] & 0x0Fu);
    h->tid_ack = (uint16_t)((p[2] >> 4) & 0x0Fu);
    h->status = (uint16_t)(p[3] & 0xFFu);
}
int trx_w_rx_crc_ok(const trx_octet *txn, unsigned payload_len)
{
    uint16_t want = trx_w_crc16(txn, TRX_W_TXN_HDR + payload_len);
    uint16_t got = (uint16_t)(((txn[TRX_W_TXN_HDR + payload_len] & 0xFFu) << 8) | (txn[TRX_W_TXN_HDR + payload_len + 1u] & 0xFFu));
    return want == got;
}
void trx_w_req_header_parse(const trx_octet *p, trx_w_req_header *h)
{
    h->len = (uint16_t)(p[0] & 0xFFu);
    h->request = (uint16_t)(p[1] & 0x7Fu);
}
void trx_w_last_status_parse(const trx_octet *p, trx_w_last_status *s)
{
    s->req = (uint16_t)(p[2] & 0x7Fu);
    s->strobe = (uint16_t)((p[2] >> 7) & 1u);
    s->tid = (uint16_t)(p[3] & 0x0Fu);
    s->status = (uint16_t)((p[3] >> 4) & 0x0Fu);
    s->params_len = (unsigned)(p[0] & 0xFFu) - TRX_W_LAST_STATUS_HDR;
}
void trx_w_cmd_status_parse(const trx_octet *p, trx_w_cmd_status *s)
{
    s->status = (uint16_t)(p[2] & 0xFFu);
    s->command_id = (uint16_t)(p[3] & 0xFFu);
    s->slot = (uint16_t)(p[4] & 0xFFu);
    s->chain_action = (uint16_t)(p[5] & 0xFFu);
    s->chain_target = (uint16_t)(p[6] & 0xFFu);
    s->params_len = (unsigned)(p[0] & 0xFFu) - TRX_W_CMD_STATUS_HDR;
}
void trx_w_stream_desc_parse(const trx_octet *p, trx_w_stream_desc *d)
{
    uint16_t w = trx_w_get16(p);
    d->stream_id = (uint16_t)(w & 0x3Fu);
    d->persisted = (uint16_t)((w >> 6) & 1u);
    d->length = trx_w_get16(p + 2);
    d->reference = trx_w_get32(p + 4);
}
