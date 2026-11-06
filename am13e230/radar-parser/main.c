/* The IWRL6432 TLV frame parser, running on the LP-AM13E230 and fed from the backchannel UART (UC4 on PA0, PA1, 115200 8N1).
   Bytes in -> tlv_feed() -> one line out per completed frame:
     F<frame> L<length> O<objects> T<tlvs> P<byte0>,<zone0> N<points> R<nearest range in mm>   (-1 = absent)
   Nothing blocks: the main loop moves bytes from the receive FIFO to the parser and from an output ring to the transmit
   FIFO, so a long print cannot overrun the receiver. Before the first byte arrives it prints a banner twice a second. */
#include "ti_sdk_dl_config.h"
#include "tlv_parser.h"

#define OUTN 4096u
static uint8_t out[OUTN]; static volatile uint32_t ow, orr;
volatile uint32_t stat_bytes, stat_frames, stat_overrun, stat_drop, stat_magic = 0x52444152u;
static tlv_parser_t P;

static void put(char c) { if (ow - orr >= OUTN) { stat_drop++; return; } out[ow++ % OUTN] = (uint8_t)c; }
static void puts_(const char *s) { while (*s) put(*s++); }
static void putu(uint32_t u)
{
    char t[11]; int n = 0;
    do { t[n++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    while (n) put(t[--n]);
}
static void putd(int32_t v) { if (v < 0) { put('-'); putu(0u - (uint32_t)v); } else putu((uint32_t)v); }
static void report(const tlv_frame_t *f)
{
    puts_("F"); putu(f->frame); puts_(" L"); putu(f->length); puts_(" O"); putu(f->objs);
    puts_(" T"); putu(f->ntlv); puts_(" P"); putd(f->pres_b0); put(','); putd(f->pres_z0);
    puts_(" N"); putu(f->points); puts_(" R"); putd(f->rmin_mm); puts_("\r\n");
}
static void drain(void)
{
    while (orr != ow && !DL_UART_isTXFIFOFull(UART_0_INST)) DL_UART_transmitData(UART_0_INST, out[orr++ % OUTN]);
}

int main(void)
{
    SYSCFG_DL_init();
    tlv_init(&P);
    uint32_t idle = 0; int started = 0;
    for (;;) {
        int busy = 0;
        while (!DL_UART_isRXFIFOEmpty(UART_0_INST)) {
            uint8_t b = DL_UART_receiveData(UART_0_INST);
            started = 1; stat_bytes++; busy = 1;
            if (tlv_feed(&P, b)) { stat_frames++; report(&P.f); }
            if (DL_UART_getRawInterruptStatus(UART_0_INST, DL_UART_INTERRUPT_OVERRUN_ERROR)) {
                stat_overrun++; DL_UART_clearInterruptStatus(UART_0_INST, DL_UART_INTERRUPT_OVERRUN_ERROR);
            }
            drain();
        }
        drain();
        if (!started && !busy && ++idle >= 8000000u) { idle = 0; puts_("RADARPARSE READY\r\n"); }
    }
}
