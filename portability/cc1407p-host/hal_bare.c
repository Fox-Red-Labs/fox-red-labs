/*
 * Bare-metal replacement for TI's two Zephyr HAL files (platform_spi.c, platform_util.c)
 * for the CC1407P host driver. Same logic as TI's Zephyr version (state bits, wait for
 * POCI low after chip select, 30 us ready poll), with the Zephyr calls replaced by the
 * hb_* hooks in hal_bare.h. No RTOS, no heap, no dummy TX buffer (the glue sends zeros).
 * Written for this project; the logic follows TI's platform_spi.c from simplelink-zephyr
 * v3.7.0-ti-1.00.01_ga.
 */
#include <stdint.h>
#include <stddef.h>
#include "hal/platform_spi.h"
#include "hal/platform_util.h"
#include "hal_bare.h"

#define ST_OPEN   0x01u
#define ST_ACTIVE 0x02u
#define ST_READY  0x04u
#define ST_CSNEW  0x08u
#define READY_POLL_US 30u
#define POCI_WAIT_US  10000u

struct Plat_SPI_Object_t {
    volatile uint8_t status;
    platSpiTransactionCompleteCallback done_cb;
    platSpiIntAssertedCallback int_cb;
    platSpiTrxReadyCallback ready_cb;
    uint8_t *tx, *rx;
    uint16_t txlen, rxlen;
    uintptr_t arg;
};
static struct Plat_SPI_Object_t obj;

/* ---- utilities ---- */
void Plat_Util_startCriticalSection(uintptr_t arg)
{
#if defined(__ARM_ARCH)
    uint32_t pm;
    __asm volatile("mrs %0, primask\n cpsid i" : "=r"(pm) :: "memory");
    *(unsigned int *)arg = pm;
#else
    (void)arg;
#endif
}
void Plat_Util_endCriticalSection(uintptr_t arg)
{
#if defined(__ARM_ARCH)
    if (!(*(unsigned int *)arg & 1u)) { __asm volatile("cpsie i" ::: "memory"); }
#else
    (void)arg;
#endif
}
/* CRC-16 ITU-T (CCITT), init 0xFFFF, as Zephyr's crc16_itu_t, bitwise to keep flash small */
uint16_t Plat_Util_computeCRC(const uint8_t *p, uint16_t n)
{
    uint16_t crc = 0xFFFFu;
    while (n--) {
        crc ^= (uint16_t)((uint16_t)*p++ << 8);
        for (int i = 0; i < 8; i++) { crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1); }
    }
    return crc;
}
void Plat_Util_blockCPU(uint32_t us) { hb_delay_us(us); }

/* ---- SPI ---- */
void Plat_SPI_Params_init(Plat_SPI_Params *p)
{
    if (p) { p->bitRate = 12000000u; p->arg = 0; p->pTransactionCompleteCb = NULL; p->pIntAssertedCb = NULL; p->pTrxReadyCallback = NULL; }
}

void hb_timer_isr(void)
{
    if (!(obj.status & ST_READY)) {
        obj.status |= ST_READY;
        if (obj.ready_cb) { obj.ready_cb(obj.arg); }
    }
}
void hb_int_isr(void)
{
    hb_timer_stop();
    obj.status |= ST_READY;
    if (obj.int_cb) { obj.int_cb(obj.arg); }
}
void hb_spi_done(void)
{
    obj.status &= (uint8_t)~ST_ACTIVE;
    if (obj.done_cb) { obj.done_cb(obj.tx, obj.txlen, obj.rx, obj.rxlen, Plat_SPI_Success, obj.arg); }
}

Plat_SPI_Handle Plat_SPI_open(const Plat_SPI_Params *p)
{
    if ((obj.status & ST_OPEN) || !p || !p->pTrxReadyCallback || !p->pIntAssertedCb || !p->pTransactionCompleteCb) { return NULL; }
    obj.done_cb = p->pTransactionCompleteCb; obj.int_cb = p->pIntAssertedCb; obj.ready_cb = p->pTrxReadyCallback; obj.arg = p->arg;
    obj.status = ST_OPEN; hb_cs(false);
    hb_reset(true); hb_delay_us(1000); hb_reset(false);
    hb_int_enable(true);
    hb_timer_oneshot_us(READY_POLL_US);
    while (!(obj.status & ST_READY)) { }
    hb_delay_us(30000u);   /* CC1407P needs 30 ms before it answers chip select */
    return &obj;
}
Plat_SPI_Status Plat_SPI_close(Plat_SPI_Handle h)
{
    if (!h) { return Plat_SPI_Invalid_Handle; }
    hb_timer_stop(); hb_int_enable(false); h->status &= (uint8_t)~ST_OPEN;
    return Plat_SPI_Success;
}
Plat_SPI_Status Plat_SPI_assertCs(Plat_SPI_Handle h)
{
    unsigned int k; Plat_SPI_Status s;
    if (!h) { return Plat_SPI_Invalid_Handle; }
    Plat_Util_startCriticalSection((uintptr_t)&k);
    if (!(h->status & ST_OPEN)) { s = Plat_SPI_Failed; }
    else if (!(h->status & ST_READY)) { s = Plat_SPI_Cs_Not_Ready; }
    else { hb_cs(true); h->status = (uint8_t)((h->status & ~ST_READY) | ST_CSNEW); s = Plat_SPI_Success; }
    Plat_Util_endCriticalSection((uintptr_t)&k);
    return s;
}
Plat_SPI_Status Plat_SPI_deassertCs(Plat_SPI_Handle h)
{
    unsigned int k;
    if (!h) { return Plat_SPI_Invalid_Handle; }
    Plat_Util_startCriticalSection((uintptr_t)&k);
    Plat_SPI_Status s = Plat_SPI_Failed;
    if (h->status & ST_OPEN) { hb_cs(false); h->status &= (uint8_t)~ST_READY; hb_timer_oneshot_us(READY_POLL_US); s = Plat_SPI_Success; }
    Plat_Util_endCriticalSection((uintptr_t)&k);
    return s;
}
bool Plat_SPI_isTrxReady(Plat_SPI_Handle h)
{
    return h && (h->status & ST_OPEN) && (h->status & ST_READY);
}
Plat_SPI_Status Plat_SPI_startTransaction(Plat_SPI_Handle h, uint8_t *tx, uint16_t txlen, uint8_t *rx, uint16_t rxlen)
{
    if (!h) { return Plat_SPI_Invalid_Handle; }
    if (h->status & ST_ACTIVE) { return Plat_SPI_Busy; }
    if (!(h->status & ST_OPEN)) { return Plat_SPI_Failed; }
    if (((txlen > 0) && !tx) || ((rxlen > 0) && !rx) || (!txlen && !rxlen)) { return Plat_SPI_Failed; }
    h->tx = tx; h->txlen = txlen; h->rx = rx; h->rxlen = rxlen;
    h->status |= ST_ACTIVE;
    if (h->status & ST_CSNEW) {
        h->status &= (uint8_t)~ST_CSNEW;
        uint32_t t = 0;
        while (hb_poci_high()) {
            hb_delay_us(1);
            if (++t >= POCI_WAIT_US) { h->status &= (uint8_t)~ST_ACTIVE; hb_cs(false); hb_timer_oneshot_us(READY_POLL_US); return Plat_SPI_Failed; }
        }
    }
    hb_spi_start(tx, rx, txlen ? txlen : rxlen);
    return Plat_SPI_Success;
}
