/* E236: DNS sinkhole for the LP-EM-CC35X1. Written fresh from RFC 1035 and the lwIP raw API; no third-party sinkhole code.
 * Blocklist: sorted 40-bit name hashes in flash (blocklist.bin from ../gen_blocklist.py), binary search per query.
 * Blocked name: A gets 0.0.0.0, AAAA gets ::, other types get an empty NOERROR answer. Other names go to the upstream
 * resolver and the answer is relayed. Counters on UART every 10 s and on a text page at http://<board>/ . */
#include "dns_boot.h"
#include "dns_checks.h"

/* hardware random numbers through the SDK's PSA crypto (HSM TRNG), already linked by the Wi-Fi stack */
typedef int32_t psa_status_t;
extern psa_status_t psa_crypto_init(void);
extern psa_status_t psa_generate_random(uint8_t *out, size_t n);

#ifndef BLK_BIN_PATH
#error "build with build.sh"
#endif
__asm__(".section .rodata,\"a\"\n.balign 4\n.global blk_table\nblk_table:\n.incbin \"" BLK_BIN_PATH "\"\n"
        ".global blk_table_end\nblk_table_end:\n.balign 4\n.global blk_table_bytes\nblk_table_bytes:\n"
        ".long blk_table_end - blk_table\n");
extern const uint8_t blk_table[];
extern const uint32_t blk_table_bytes;

#define NSLOT 16
#define SLOT_MS 2500
typedef struct { ip_addr_t cip; u16_t cport, cid, uid, qslen; u32_t t; uint8_t used; uint8_t qs[260], oqs[260]; } slot_t;
static slot_t slots[NSLOT];
static struct udp_pcb *srv, *up[NSLOT];   /* one pcb per slot, moved to a fresh random port for every forwarded query */
static ip_addr_t upstream;
static uint32_t c_q, c_blocked, c_fwd, c_relay, c_timeout, c_drop, c_nodata, c_foreign, c_badreply, c_big;
static int trng_ok;

static uint64_t hash_name(const char *s)
{
    uint64_t h = 0xCBF29CE484222325ULL;
    for (; *s; s++) h = (h ^ (uint8_t)*s) * 0x100000001B3ULL;
    return (h ^ (h >> 40)) & 0xFFFFFFFFFFULL;
}

static uint64_t rd40(uint32_t i)
{
    const uint8_t *p = blk_table + 5u * i;
    return ((uint64_t)p[0] << 32) | ((uint64_t)p[1] << 24) | (p[2] << 16) | (p[3] << 8) | p[4];
}

static int blocked(const char *name)
{
    uint64_t k = hash_name(name);
    uint32_t lo = 0, hi = blk_table_bytes / 5;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        uint64_t v = rd40(mid);
        if (v == k) return 1;
        if (v < k) lo = mid + 1; else hi = mid;
    }
    return 0;
}

/* Question name to lower-case dotted text. Returns the offset after the name's 0 byte, or 0 if malformed. */
static uint16_t read_name(const uint8_t *q, uint16_t n, char *out)
{
    uint16_t i = 12, o = 0;
    while (i < n && q[i]) {
        uint8_t l = q[i++];
        if (l > 63 || i + l >= n || o + l + 1 >= 255) return 0;
        if (o) out[o++] = '.';
        for (; l; l--, i++) out[o++] = (q[i] >= 'A' && q[i] <= 'Z') ? q[i] + 32 : q[i];
    }
    out[o] = 0;
    return (i < n && o) ? i + 1 : 0;
}

static void answer_blocked(struct udp_pcb *pcb, const uint8_t *q, uint16_t qend, uint16_t qtype, const ip_addr_t *a, u16_t port)
{
    uint16_t add = (qtype == 1) ? 16 : (qtype == 28) ? 28 : 0;
    struct pbuf *r = pbuf_alloc(PBUF_TRANSPORT, qend + add, PBUF_RAM);
    if (!r) { c_drop++; return; }
    uint8_t *o = r->payload;
    memcpy(o, q, qend);
    o[2] = 0x80 | (q[2] & 0x79); o[3] = 0x80;
    o[6] = 0; o[7] = add ? 1 : 0; o[8] = o[9] = o[10] = o[11] = 0;
    if (add) {
        memset(o + qend, 0, add);
        o[qend] = 0xC0; o[qend + 1] = 0x0C; o[qend + 3] = qtype; o[qend + 5] = 1; o[qend + 9] = 60; o[qend + 11] = (qtype == 1) ? 4 : 16;
    } else c_nodata++;
    udp_sendto(pcb, r, a, port);
    pbuf_free(r);
}

static void srv_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    uint8_t q[300];
    char name[256];
    uint16_t n = p->tot_len, got = n < sizeof q ? n : sizeof q;
    (void)arg;
    pbuf_copy_partial(p, q, got, 0);
    c_q++;
    if (!IP_IS_V4(addr) || !dns_same_subnet(ip_addr_get_ip4_u32(addr), gIp4Addr, gIp4Mask)) { pbuf_free(p); c_foreign++; return; }
    if (n > DNS_MAX_QUERY) { pbuf_free(p); c_drop++; return; }
    if (n < 17 || (q[2] & 0xF8) || q[4] || q[5] != 1) { pbuf_free(p); c_drop++; return; }
    uint16_t qend = read_name(q, got, name);
    if (!qend || qend + 4 > got) { pbuf_free(p); c_drop++; return; }
    uint16_t qtype = (q[qend] << 8) | q[qend + 1];
    if (blocked(name)) {
        c_blocked++;
        answer_blocked(pcb, q, qend + 4, qtype, addr, port);
        pbuf_free(p);
        return;
    }
    /* forward: find a free or expired slot */
    uint32_t now = sys_now();
    int s = -1;
    for (int i = 0; i < NSLOT; i++) {
        if (slots[i].used && now - slots[i].t > SLOT_MS) { slots[i].used = 0; c_timeout++; }
        if (!slots[i].used && s < 0) s = i;
    }
    struct pbuf *f = (s >= 0) ? pbuf_alloc(PBUF_TRANSPORT, n, PBUF_RAM) : NULL;
    if (!f) { pbuf_free(p); c_drop++; return; }
    pbuf_copy(f, p);
    if (!trng_ok) { pbuf_free(f); pbuf_free(p); c_drop++; return; }      /* fail closed: no random IDs, no forwarding */
    uint16_t uid;
    for (int tries = 0;; tries++) {
        uint8_t rb[2];
        if (psa_generate_random(rb, 2) != 0 || tries > 8) { pbuf_free(f); pbuf_free(p); c_drop++; return; }
        uid = (rb[0] << 8) | rb[1];
        int dup = 0;
        for (int i = 0; i < NSLOT; i++) if (slots[i].used && slots[i].uid == uid) dup = 1;
        if (!dup) break;
    }
    slots[s].uid = uid;
    slots[s].qslen = qend + 4 - 12;
    memcpy(slots[s].oqs, q + 12, slots[s].qslen);
    memcpy(slots[s].qs, q + 12, slots[s].qslen);
    {   /* 0x20: random upper/lower case on each letter of the forwarded name; the reply must echo it exactly */
        uint8_t cb[32];
        uint16_t i = 0, bit = 0;
        if (psa_generate_random(cb, sizeof cb) != 0) { pbuf_free(f); pbuf_free(p); c_drop++; return; }
        while (i < slots[s].qslen && slots[s].qs[i]) {
            uint8_t l = slots[s].qs[i++];
            for (; l; l--, i++, bit++) {
                uint8_t c = slots[s].qs[i];
                if (((c | 32) >= 'a' && (c | 32) <= 'z') && ((cb[(bit >> 3) & 31] >> (bit & 7)) & 1)) slots[s].qs[i] = c ^ 32;
                pbuf_put_at(f, 12 + i, slots[s].qs[i]);
            }
        }
    }
    /* fresh random source port for this query */
    for (int tries = 0;; tries++) {
        uint8_t pb[2];
        if (tries > 8 || psa_generate_random(pb, 2) != 0) { pbuf_free(f); pbuf_free(p); c_drop++; return; }
        if (udp_bind(up[s], IP_ANY_TYPE, 49152 + (((pb[0] << 8) | pb[1]) % 16384)) == ERR_OK) break;
    }
    slots[s].cid = (q[0] << 8) | q[1];
    slots[s].cip = *addr; slots[s].cport = port; slots[s].t = now; slots[s].used = 1;
    pbuf_put_at(f, 0, slots[s].uid >> 8); pbuf_put_at(f, 1, slots[s].uid & 0xFF);
    pbuf_free(p);
    if (udp_sendto(up[s], f, &upstream, 53) == ERR_OK) c_fwd++; else { slots[s].used = 0; c_drop++; }
    pbuf_free(f);
}

static void up_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    uint8_t h[300];
    int s = (int)(intptr_t)arg;   /* the pcb is this slot's: a reply on any other local port never reaches this slot */
    (void)pcb;
    if (port == 53 && ip_addr_cmp(addr, &upstream) && p->tot_len >= 12 && p->tot_len <= DNS_MAX_RESP && slots[s].used) {
        uint16_t id = (pbuf_get_at(p, 0) << 8) | pbuf_get_at(p, 1), got = p->tot_len < sizeof h ? p->tot_len : sizeof h;
        pbuf_copy_partial(p, h, got, 0);
        if (id == slots[s].uid && dns_reply_matches(h, got, slots[s].qs, slots[s].qslen, 1)) {
            struct pbuf *r = pbuf_alloc(PBUF_TRANSPORT, p->tot_len, PBUF_RAM);
            if (r) {
                pbuf_copy(r, p);
                pbuf_put_at(r, 0, slots[s].cid >> 8); pbuf_put_at(r, 1, slots[s].cid & 0xFF);
                for (uint16_t k = 0; k < slots[s].qslen; k++) pbuf_put_at(r, 12 + k, slots[s].oqs[k]);   /* give the client its own case back */
                udp_sendto(srv, r, &slots[s].cip, slots[s].cport);
                pbuf_free(r);
                slots[s].used = 0;
                c_relay++;
            }
        } else c_badreply++;
    } else if (p->tot_len > DNS_MAX_RESP) c_big++;
    else c_badreply++;
    pbuf_free(p);
}

/* status page */
static err_t http_recv(void *arg, struct tcp_pcb *t, struct pbuf *p, err_t err)
{
    (void)arg;
    if (!p) { tcp_close(t); return ERR_OK; }
    tcp_recved(t, p->tot_len);
    pbuf_free(p);
    char body[260], resp[380];
    int bl = snprintf(body, sizeof body, "queries %u\nblocked %u\nforwarded %u\nrelayed %u\ntimeouts %u\ndropped %u\nnodata %u\nforeign_src %u\nbad_reply %u\noversize %u\nlist_domains %u\n",
                      (unsigned)c_q, (unsigned)c_blocked, (unsigned)c_fwd, (unsigned)c_relay, (unsigned)c_timeout, (unsigned)c_drop,
                      (unsigned)c_nodata, (unsigned)c_foreign, (unsigned)c_badreply, (unsigned)c_big, (unsigned)(blk_table_bytes / 5));
    int rl = snprintf(resp, sizeof resp, "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s", bl, body);
    tcp_write(t, resp, rl, TCP_WRITE_FLAG_COPY);
    tcp_output(t);
    tcp_close(t);
    return ERR_OK;
}

static err_t http_accept(void *arg, struct tcp_pcb *t, err_t err)
{
    (void)arg; (void)err;
    tcp_recv(t, http_recv);
    return ERR_OK;
}

static void start_cb(void *arg)
{
    (void)arg;
    srv = udp_new();
    err_t e = srv ? udp_bind(srv, IP_ANY_TYPE, 53) : ERR_MEM;
    err_t e2 = ERR_OK;
    for (int i = 0; i < NSLOT; i++) {
        up[i] = udp_new();
        if (!up[i]) { e2 = ERR_MEM; break; }
        udp_recv(up[i], up_recv, (void *)(intptr_t)i);
    }
    if (e == ERR_OK && e2 == ERR_OK) udp_recv(srv, srv_recv, NULL);
    struct tcp_pcb *l = tcp_new();
    if (l && tcp_bind(l, IP_ANY_TYPE, 80) == ERR_OK) { l = tcp_listen(l); tcp_accept(l, http_accept); }
    UART_PRINT("udp 53 bind %d, upstream bind %d, tcp 80 %s\r\n", (int)e, (int)e2, l ? "up" : "no");
}

void *mainThread(void *args)
{
    if (dns_boot("E236 DNS sinkhole") != 0) return NULL;
    trng_ok = (psa_crypto_init() == 0);
    UART_PRINT("hardware random IDs: %s\r\n", trng_ok ? "yes" : "NO, forwarding disabled");
#ifdef DNS_UPSTREAM_STR
    ipaddr_aton(DNS_UPSTREAM_STR, &upstream);
#else
    ip_addr_set_ip4_u32(&upstream, gIp4GW);
#endif
    UART_PRINT("list %u domains (%u bytes in flash), upstream %s\r\n", (unsigned)(blk_table_bytes / 5), (unsigned)blk_table_bytes, ipaddr_ntoa(&upstream));
    tcpip_callback(start_cb, NULL);
    for (;;) {
        osi_Sleep(10);
        UART_PRINT("q %u blocked %u fwd %u relay %u timeout %u drop %u foreign %u badreply %u heap %u\r\n", (unsigned)c_q, (unsigned)c_blocked, (unsigned)c_fwd,
                   (unsigned)c_relay, (unsigned)c_timeout, (unsigned)c_drop, (unsigned)c_foreign, (unsigned)c_badreply, (unsigned)osi_GetFreeHeapSize());
    }
}
