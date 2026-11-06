/* E236 stage 1: the smallest DNS server. Binds UDP 53 and answers every A query with the fixed address 192.0.2.1.
 * Written fresh from RFC 1035. Build with ../build.sh (APP=min). */
#include "dns_boot.h"

static uint32_t n_q;

static void min_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    uint8_t q[300];
    uint16_t n = p->tot_len, i = 12;
    (void)arg;
    if (n < 17 || n > sizeof q) { pbuf_free(p); return; }
    pbuf_copy_partial(p, q, n, 0);
    pbuf_free(p);
    if ((q[2] & 0x80) || q[4] || q[5] != 1) return;           /* a response, or not exactly one question */
    while (i < n && q[i]) i += q[i] + 1;                      /* walk the name */
    if (i + 5 > n) return;
    uint16_t qend = i + 5;                                    /* name end 0, type 2, class 2 */
    uint16_t qtype = (q[i + 1] << 8) | q[i + 2];
    n_q++;
    struct pbuf *r = pbuf_alloc(PBUF_TRANSPORT, qend + 16, PBUF_RAM);
    if (!r) return;
    uint8_t *o = r->payload;
    memcpy(o, q, qend);
    o[2] = 0x80 | (q[2] & 0x79);                              /* QR, keep opcode and RD */
    o[3] = 0x80;                                              /* RA, rcode 0 */
    o[6] = 0; o[7] = (qtype == 1) ? 1 : 0; o[8] = o[9] = o[10] = o[11] = 0;
    uint16_t len = qend;
    if (qtype == 1) {
        static const uint8_t a[] = {0xC0, 0x0C, 0, 1, 0, 1, 0, 0, 0, 60, 0, 4, 192, 0, 2, 1};
        memcpy(o + len, a, sizeof a);
        len += sizeof a;
    }
    pbuf_realloc(r, len);
    udp_sendto(pcb, r, addr, port);
    pbuf_free(r);
}

static void bind_cb(void *arg)
{
    struct udp_pcb *pcb = udp_new();
    err_t e = pcb ? udp_bind(pcb, IP_ANY_TYPE, 53) : ERR_MEM;
    UART_PRINT("udp_bind 53: %d\r\n", (int)e);
    if (e == ERR_OK) udp_recv(pcb, min_recv, NULL);
}

void *mainThread(void *args)
{
    if (dns_boot("E236 DNS stage 1") != 0) return NULL;
    tcpip_callback(bind_cb, NULL);
    for (;;) {
        osi_Sleep(10);
        UART_PRINT("listening on %s:53, queries %u\r\n", ip4str(gIp4Addr), (unsigned)n_q);
    }
}
