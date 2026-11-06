/*
 * Packer test, one source for the Mac and the C28x. Rebuilds every reference vector (vectors.h, made from TI's
 * structs on a byte-addressed host) with the field-by-field packer and counts mismatches.
 *   Mac:   cc ... -DHOST_BUILD test_wire.c trx_wire.c   prints the result and writes stream.bin
 *   C28x:  results are globals read through the debugger: wire_cases, wire_fail, wire_digest, wire_out[]
 * wire_out collects every packed vector back to back, so its bytes can be compared with the Mac's stream.bin.
 */
#include <stdint.h>
#include "trx_wire.h"
#include "scenario.h"
#include "vectors.h"
#include "rcl_settings_wisun.h"

#define OUT_MAX 1100u
volatile uint16_t wire_cases, wire_fail, wire_out_n, wire_done, wire_fail_mask_lo, wire_fail_mask_hi;
volatile uint32_t wire_digest;
volatile uint16_t wire_out[OUT_MAX];     /* low 8 bits of each octet */
static trx_octet buf[160];
static trx_octet tab[4u * SC_MAIN_WORDS];   /* larger of the two settings tables */

static void note(const trx_octet *b, unsigned n)
{
    unsigned i;
    for (i = 0; i < n; i++) {
        if (wire_out_n < OUT_MAX) { wire_out[wire_out_n++] = (uint16_t)(b[i] & 0xFFu); }
        wire_digest = (wire_digest ^ (b[i] & 0xFFu)) * 16777619UL;
    }
}
static void check(unsigned idx, const trx_octet *got, unsigned got_n, const unsigned char *want, unsigned want_n)
{
    unsigned i, bad = (got_n != want_n);
    for (i = 0; i < want_n && i < got_n; i++) { if ((got[i] & 0xFFu) != (want[i] & 0xFFu)) { bad = 1; } }
    wire_cases++;
    if (bad) {
        wire_fail++;
        if (idx < 16u) { wire_fail_mask_lo |= (uint16_t)(1u << idx); } else { wire_fail_mask_hi |= (uint16_t)(1u << (idx - 16u)); }
    }
    note(got, got_n);
}
static uint32_t fnv(const trx_octet *b, unsigned n)
{
    uint32_t h = 2166136261UL;
    unsigned i;
    for (i = 0; i < n; i++) { h ^= (uint32_t)(b[i] & 0xFFu); h *= 16777619UL; }
    return h;
}
static void expect32(unsigned idx, uint32_t got, uint32_t want)
{
    trx_octet g[4];
    trx_octet w[4];
    trx_w_put32(g, got);
    trx_w_put32(w, want);
    { unsigned char ww[4]; unsigned i; for (i = 0; i < 4u; i++) { ww[i] = (unsigned char)w[i]; } check(idx, g, 4u, ww, 4u); }
}

void wire_run(void)
{
    unsigned n, i, fe_n;
    trx_w_devcfg dc;
    trx_w_cmd_tx cmd;
    unsigned idx = 0;

    n = trx_w_req_config_list(buf);                     check(idx++, buf, n, V_CONFIG_LIST, V_CONFIG_LIST_LEN);
    n = trx_w_req_nv_erase(buf);                        check(idx++, buf, n, V_NV_ERASE, V_NV_ERASE_LEN);
    n = trx_w_req_ping(buf, 0xA5u);                     check(idx++, buf, n, V_PING, V_PING_LEN);

    dc.dio[0] = SC_DIO0; dc.dio[1] = SC_DIO2; dc.dio[2] = SC_DIO3; dc.dio[3] = SC_DIO4; dc.dio[4] = SC_DIO5;
    dc.dio[5] = SC_DIO6; dc.dio[6] = SC_DIO7; dc.clock_config = SC_CLOCK; dc.dwell_time_us = SC_DWELL;
    dc.power_policy = SC_POLICY; dc.rf_mode = SC_RFMODE;
    n = trx_w_req_devcfg_store(buf, &dc);               check(idx++, buf, n, V_DEVCFG, V_DEVCFG_LEN);

    /* settings tables: serialise the uint32 arrays; byte count is 4 per word, not sizeof */
    fe_n = trx_w_words_to_octets(tab, LRF_frontendRegConfig_wisun, SC_FE_WORDS);
    expect32(idx++, fnv(tab, fe_n), SC_FE_FNV);
    n = trx_w_req_config_store(buf, 0u, 1u, 1u, SC_FE_REF, tab, SC_FRAG1_LEN);
    check(idx++, buf, n, V_CFG_STORE_FIRST, V_CFG_STORE_FIRST_LEN);
    n = trx_w_req_config_store(buf, 0u, 0u, 0u, SC_FE_REF, tab + (fe_n - SC_FRAG2_LEN), SC_FRAG2_LEN);
    check(idx++, buf, n, V_CFG_STORE_LAST, V_CFG_STORE_LAST_LEN);
    n = trx_w_req_config_persist(buf, 1u);              check(idx++, buf, n, V_CFG_PERSIST, V_CFG_PERSIST_LEN);
    n = trx_w_words_to_octets(tab, LRF_mainRegConfig_wisun, SC_MAIN_WORDS);
    expect32(idx++, fnv(tab, n), SC_MAIN_FNV);

    cmd.slot = SC_CMD_SLOT; cmd.cmd_id = 1u; cmd.slot_on_true = 3u; cmd.enable_on_true = 1u; cmd.slot_on_false = 5u;
    cmd.enable_on_false = 0u; cmd.slot_on_compare = 7u; cmd.enable_on_compare = 2u; cmd.next_status_compare = -2;
    cmd.trigger = 0u; cmd.conflict_policy = 0u; cmd.allow_delay = 1u; cmd.chain_trigger = 10u;
    cmd.chain_conflict_policy = 2u; cmd.chain_allow_delay = 1u; cmd.trigger_param = 0x11223344UL;
    cmd.chain_trig_param = 0x55667788UL;
    cmd.phy[0].option_mask = SC_PHY_MASK; cmd.phy[0].config_id = SC_PHY_CFGID;
    cmd.phy[1].option_mask = 0x1234u; cmd.phy[1].config_id = 9u;
    cmd.phy[2].option_mask = 0xBEEFu; cmd.phy[2].config_id = 15u;
    cmd.stream_id = SC_STREAM_ID; cmd.frequency_khz = SC_FREQ_KHZ; cmd.pa = SC_PA; cmd.modem = SC_MODEM;
    cmd.power = trx_w_txpower(SC_PWR_FRACTION, SC_PWR_DBM);
    if (cmd.power != SC_PWR_RAW) { wire_fail++; }
    n = trx_w_req_cmd_store_tx(buf, &cmd);              check(idx++, buf, n, V_CMD_STORE, V_CMD_STORE_LEN);
    n = trx_w_req_cmd_submit(buf, 4u);                  check(idx++, buf, n, V_CMD_SUBMIT, V_CMD_SUBMIT_LEN);
    n = trx_w_req_cmd_stop(buf, 2u, 2u);                check(idx++, buf, n, V_CMD_STOP, V_CMD_STOP_LEN);
    n = trx_w_payload_hdr_sunfsk(buf, SC_PAYLOAD_LEN, 1u, 1u, 0u, 0u, 0x2Cu);
    check(idx++, buf, n, V_PAYLOAD_HDR, V_PAYLOAD_HDR_LEN);
    {
        trx_octet pkt[4u + SC_PAYLOAD_LEN];
        trx_w_payload_hdr_sunfsk(pkt, SC_PAYLOAD_LEN, 1u, 1u, 0u, 0u, 0x2Cu);
        for (i = 0; i < SC_PAYLOAD_LEN; i++) { pkt[4u + i] = (trx_octet)SC_PAY_BYTE(i); }
        n = trx_w_req_stream_store(buf, 5u, 1u, 0u, 1u, pkt, 4u + SC_PAYLOAD_LEN);
        check(idx++, buf, n, V_STREAM_STORE, V_STREAM_STORE_LEN);
    }
    /* framed transactions */
    {
        trx_octet t[140];
        unsigned len = 0, tot;
        n = trx_w_req_config_list(t + TRX_W_TXN_HDR);
        len = trx_w_pad4(t + TRX_W_TXN_HDR, n);
        trx_w_txn_header(t, 0u, 0u, 1u, 1u, 0u);
        tot = trx_w_txn_finish(t, len, 1u, 1u);
        check(idx++, t, tot, V_DRIVER_TXN1, V_DRIVER_TXN1_LEN);

        len = trx_w_req_devcfg_store(t + TRX_W_TXN_HDR, &dc);
        len = trx_w_pad4(t + TRX_W_TXN_HDR, len);
        len += trx_w_req_nv_erase(t + TRX_W_TXN_HDR + len);
        len = trx_w_pad4(t + TRX_W_TXN_HDR, len);
        trx_w_txn_header(t, 0u, 0u, 2u, 1u, 0u);
        tot = trx_w_txn_finish(t, len, 2u, 1u);
        check(idx++, t, tot, V_TXN_DEVCFG_NVERASE, V_TXN_DEVCFG_NVERASE_LEN);
    }
    /* radio to host */
    {
        trx_octet r[64];
        trx_w_rx_header h;
        trx_w_req_header rh;
        trx_w_last_status ls;
        trx_w_cmd_status cs;
        trx_w_stream_desc sd;
        unsigned ok = 1;
        for (i = 0; i < V_RX_TXN_LEN; i++) { r[i] = (trx_octet)V_RX_TXN[i]; }
        trx_w_rx_header_parse(r, &h);
        if (h.free_blocks != V_RX_FREE || h.error != V_RX_ERROR || h.len != V_RX_LEN || h.tid_tx != V_RX_TIDTX ||
            h.tid_ack != V_RX_TIDACK || h.status != V_RX_STATUS || h.data_rdy_n != 0u || h.chip_rdy_n != 0u) { ok = 0; }
        if (!trx_w_rx_crc_ok(r, h.len)) { ok = 0; }
        r[V_RX_TXN_LEN - 1u] ^= 1u;
        if (trx_w_rx_crc_ok(r, h.len)) { ok = 0; }          /* a flipped bit must fail */
        r[V_RX_TXN_LEN - 1u] ^= 1u;
        trx_w_req_header_parse(r + TRX_W_TXN_HDR + V_LS_OFF, &rh);
        if (rh.request != TRX_REQ_LAST_STATUS) { ok = 0; }
        trx_w_last_status_parse(r + TRX_W_TXN_HDR + V_LS_OFF, &ls);
        if (ls.req != V_LS_REQ || ls.tid != V_LS_TID || ls.status != V_LS_STATUS || ls.strobe != 0u || ls.params_len != 1u ||
            (r[TRX_W_TXN_HDR + V_LS_OFF + TRX_W_LAST_STATUS_HDR] & 0xFFu) != V_LS_PARAM) { ok = 0; }
        trx_w_cmd_status_parse(r + TRX_W_TXN_HDR + V_CS_OFF, &cs);
        if (cs.status != V_CS_STATUS || cs.command_id != V_CS_CMD || cs.slot != V_CS_SLOT || cs.chain_action != V_CS_CHAIN ||
            cs.chain_target != V_CS_TARGET) { ok = 0; }
        for (i = 0; i < 8u; i++) { r[i] = (trx_octet)V_STREAM_DESC[i]; }
        trx_w_stream_desc_parse(r, &sd);
        if (sd.stream_id != 37u || sd.persisted != 1u || sd.length != 1234u || sd.reference != 0xCAFEF00DUL) { ok = 0; }
        wire_cases++;
        if (!ok) { wire_fail++; wire_fail_mask_hi |= 0x8000u; }
    }
    wire_done = 1u;
}

#ifdef HOST_BUILD
#include <stdio.h>
int main(void)
{
    FILE *f;
    unsigned i;
    wire_run();
    printf("cases %u fail %u octets %u digest %08lX\n", wire_cases, wire_fail, wire_out_n, (unsigned long)wire_digest);
    f = fopen("stream.bin", "wb");
    for (i = 0; i < wire_out_n; i++) { fputc(wire_out[i], f); }
    fclose(f);
    return wire_fail != 0;
}
#endif
