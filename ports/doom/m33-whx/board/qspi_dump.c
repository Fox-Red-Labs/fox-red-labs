/* E27 step 1 and 3: read-only QSPI flash dump firmware for the LP-MSPM33C321A.
 * Commands sent to the flash: 0x9F (read ID), 0x05 (status 1), 0x15 (config register, Macronix only), 0x5A (SFDP),
 * 0x03 (normal read), 0x0B (fast read). No write enable, no program, no erase, no status write, no 0x35.
 * The host writes 0xA5000000 | (seq << 8) | block (DSLite load of 4 bytes at MBOX, which restarts the program; MBOX is outside every linked section so it survives); the firmware reads one 128 kB block with 0x0B into buf,
 * computes CRC-32 on the chip and sets qd.done = qd.req. The host reads qd and buf over the probe.
 * Written fresh from the SDK driver API (source/ti/external_flash/QSPI_FLASH.h). */
#include <string.h>
#include <ti/external_flash/QSPI_FLASH.h>
#include "ti_msp_dl_config.h"

#define BLK (128U * 1024U)
#define NBLK 128U   /* the part is 16 MB (ID C2 20 18) */
uint8_t buf[BLK] __attribute__((aligned(4)));

typedef struct {
    uint32_t magic;            /* 0xD00D0001 when init and the identify part are done */
    uint32_t err;              /* 0 or the step that failed */
    uint8_t id[4];
    uint8_t sr1, cfg, pad0, pad1;
    uint8_t sfdp[64];
    uint8_t fast64[64];
    uint8_t norm64[64];
    uint32_t norm_eq_fast;
    volatile uint32_t req;     /* host writes (seq << 8) | block in one word, so the request is atomic */
    volatile uint32_t done, done_blk, crc, ticks, rd_err;
} qd_t;
volatile qd_t qd;
#define MBOX (*(volatile uint32_t *) 0x30030000u)   /* outside .bss and the stack (see the map); not cleared at start-up */

static QSPIFLASH_Handle h;
static QSPIFLASH_InitParams ip;

static uint32_t crc32(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

static QSPIFLASH_Status rd(uint8_t cmd, uint32_t addr, uint32_t dummy, void *dst, uint32_t n)
{
    QSPIFLASH_CommandConfig c = {0};
    c.instruction  = cmd;
    c.address      = addr;
    c.addressSize  = QSPIFLASH_ADDRESS_SIZE_3BYTE;
    c.dummyCycles  = dummy;
    c.transferMode = QSPIFLASH_TRANSFER_MODE_SPI;
    return QSPIFLASH_read(&h, dst, n, &c);
}

int main(void)
{
    SYSCFG_DL_init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    ip.qspiRegs = QSPI;
    if (QSPIFLASH_init(&h, &ip) != QSPIFLASH_STATUS_SUCCESS) { qd.err = 1; goto idle; }
    QSPIFLASH_CommandConfig c = {0};
    c.transferMode = QSPIFLASH_TRANSFER_MODE_SPI_HALF_DUPLEX;
    c.instruction  = READ_ID_CMD;
    uint8_t id[4] = {0};
    if (QSPIFLASH_readID(&h, id, 3, &c) != QSPIFLASH_STATUS_SUCCESS) { qd.err = 2; goto idle; }
    memcpy((void *)qd.id, id, 4);
    c.instruction = READ_STATUS_REG_CMD;
    uint8_t s = 0;
    if (QSPIFLASH_readStatusRegister(&h, &s, &c) != QSPIFLASH_STATUS_SUCCESS) { qd.err = 3; goto idle; }
    qd.sr1 = s;
    if (id[0] == 0xC2) {   /* Macronix: 0x15 reads the configuration register; 0x35 would enter QPI mode, so it is never sent */
        c.instruction = READ_CFG_REG_CMD;
        if (QSPIFLASH_readStatusRegister(&h, &s, &c) == QSPIFLASH_STATUS_SUCCESS) qd.cfg = s;
    }
    if (rd(READ_SFDP_TABLE_CMD, 0, 8, buf, 64) != QSPIFLASH_STATUS_SUCCESS) { qd.err = 4; goto idle; }
    memcpy((void *)qd.sfdp, buf, 64);
    if (rd(FAST_READ_CMD, 0, DUMMY_CYCLES_FAST_READ, buf, 64) != QSPIFLASH_STATUS_SUCCESS) { qd.err = 5; goto idle; }
    memcpy((void *)qd.fast64, buf, 64);
    if (rd(NORMAL_READ_CMD, 0, 0, buf, 64) != QSPIFLASH_STATUS_SUCCESS) { qd.err = 6; goto idle; }
    memcpy((void *)qd.norm64, buf, 64);
    qd.norm_eq_fast = memcmp((const void *)qd.fast64, (const void *)qd.norm64, 64) == 0;
    qd.magic = 0xD00D0001u;
idle:
    for (;;) {
        uint32_t r = MBOX;
        if ((r >> 24) == 0xA5u && r != qd.done && qd.magic == 0xD00D0001u) {
            uint32_t seq = r, blk = r & 0xFFu;
            qd.req = r;
            uint32_t t0 = DWT->CYCCNT;
            qd.rd_err = (blk < NBLK) ? (uint32_t) rd(FAST_READ_CMD, blk * BLK, DUMMY_CYCLES_FAST_READ, buf, BLK) : 99U;
            qd.ticks = DWT->CYCCNT - t0;
            qd.crc = crc32(buf, BLK);
            qd.done_blk = blk;
            qd.done = seq;
        }
    }
}
