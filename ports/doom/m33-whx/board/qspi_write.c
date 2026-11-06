/* E27 step 5: write the Doom data file (WHX) into the blank part of the LP-MSPM33C321A's QSPI flash. Guarded.
 * Commands sent to the flash: 0x9F, 0x05 (reads), 0x0B (fast read, blank check and read-back), 0x06 (write enable), 0x02 (page program,
 * 256 bytes), and 0x05 polling for the busy bit. Never 0x20, 0x52, 0xD8, 0xC7 (erase), never 0x01 (status write), never 0x35.
 * Guards, all checked on the chip before any write: the flash ID is C2 20 18; no block-protect bit is set; the target is inside
 * [WIN_LO, WIN_HI); the staged data matches the host's CRC-32; the whole target range reads back as 0xFF (or already equals the data).
 * After the programming, the range is read back and compared with the staged data page by page.
 * The host stages the data and the request with one DSLite load: STG (128 kB) then the header at HDR = STG + 0x20000, which starts
 * the program (a load restarts it). Both are outside every linked section, so they survive the restart.
 * Written fresh from the SDK driver API (source/ti/external_flash/QSPI_FLASH.h). */
#include <string.h>
#include <ti/external_flash/QSPI_FLASH.h>
#include "ti_msp_dl_config.h"

#define WIN_LO 0x00640000u              /* the blank run chosen from the dump (E27 step 4): 28 blocks of 64 kB */
#define WIN_HI (WIN_LO + 0x1C0000u)
#define STG ((volatile uint8_t *) 0x30010000u)
typedef struct { uint32_t addr, len, crc, req; } hdr_t;
#define HDR (*(volatile hdr_t *) 0x30030000u)

typedef struct {
    uint32_t magic, err, id, sr1, sr1_after;
    uint32_t done, done_addr, done_len, pages, blank_ok, already, verify_bad, ticks;
} wr_t;
volatile wr_t wr;
static uint8_t scratch[256] __attribute__((aligned(4)));
static QSPIFLASH_Handle h;
static QSPIFLASH_InitParams ip;

static uint32_t crc32(const volatile uint8_t *p, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    while (n--) { c ^= *p++; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); }
    return ~c;
}
static QSPIFLASH_Status rd(uint32_t addr, void *dst, uint32_t n)
{
    QSPIFLASH_CommandConfig c = {0};
    c.instruction = FAST_READ_CMD; c.address = addr; c.addressSize = QSPIFLASH_ADDRESS_SIZE_3BYTE;
    c.dummyCycles = DUMMY_CYCLES_FAST_READ; c.transferMode = QSPIFLASH_TRANSFER_MODE_SPI;
    return QSPIFLASH_read(&h, dst, n, &c);
}
static uint8_t sr1(void)
{
    QSPIFLASH_CommandConfig c = {0}; uint8_t s = 0xFF;
    c.instruction = READ_STATUS_REG_CMD; c.transferMode = QSPIFLASH_TRANSFER_MODE_SPI_HALF_DUPLEX;
    QSPIFLASH_readStatusRegister(&h, &s, &c); return s;
}

int main(void)
{
    SYSCFG_DL_init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    ip.qspiRegs = QSPI;
    if (QSPIFLASH_init(&h, &ip) != QSPIFLASH_STATUS_SUCCESS) { wr.err = 1; goto idle; }
    QSPIFLASH_CommandConfig c = {0}; uint8_t id[3] = {0};
    c.instruction = READ_ID_CMD; c.transferMode = QSPIFLASH_TRANSFER_MODE_SPI_HALF_DUPLEX;
    if (QSPIFLASH_readID(&h, id, 3, &c) != QSPIFLASH_STATUS_SUCCESS) { wr.err = 2; goto idle; }
    wr.id = ((uint32_t) id[0] << 16) | ((uint32_t) id[1] << 8) | id[2];
    if (wr.id != 0xC22018u) { wr.err = 3; goto idle; }
    wr.sr1 = sr1();
    if (wr.sr1 & 0x3Cu) { wr.err = 4; goto idle; }       /* a block-protect bit is set: stop */
    wr.magic = 0xD00D0002u;
idle:
    for (;;) {
        uint32_t r = HDR.req;
        if (wr.magic != 0xD00D0002u || (r >> 24) != 0xA5u || r == wr.done) continue;
        uint32_t a = HDR.addr, n = HDR.len, t0 = DWT->CYCCNT;
        wr.err = 0; wr.verify_bad = 0; wr.blank_ok = 0; wr.already = 0; wr.pages = 0;
        if (a < WIN_LO || n == 0 || n > 0x20000u || a + n > WIN_HI || (a & 0xFFu)) { wr.err = 90; wr.done = r; continue; }
        if (crc32(STG, n) != HDR.crc) { wr.err = 91; wr.done = r; continue; }
        uint32_t same = 1, blank = 1;
        for (uint32_t o = 0; o < n; o += 256) {
            uint32_t k = n - o > 256 ? 256 : n - o;
            if (rd(a + o, scratch, k) != QSPIFLASH_STATUS_SUCCESS) { wr.err = 92; break; }
            for (uint32_t i = 0; i < k; i++) { if (scratch[i] != 0xFF) blank = 0; if (scratch[i] != STG[o + i]) same = 0; }
        }
        if (wr.err) { wr.done = r; continue; }
        wr.blank_ok = blank; wr.already = same;
        if (!blank && !same) { wr.err = 93; wr.done = r; continue; }   /* not blank and not our data: never write over it */
        if (!same) {
            for (uint32_t o = 0; o < n; o += 256) {
                uint32_t k = n - o > 256 ? 256 : n - o;
                QSPIFLASH_CommandConfig w = {0};
                w.instruction = PAGE_PROGRAM_CMD; w.address = a + o; w.addressSize = QSPIFLASH_ADDRESS_SIZE_3BYTE;
                w.transferMode = QSPIFLASH_TRANSFER_MODE_SPI_HALF_DUPLEX;
                memcpy(scratch, (const void *) (STG + o), k);
                if (QSPIFLASH_write(&h, scratch, k, &w) != QSPIFLASH_STATUS_SUCCESS) { wr.err = 94; break; }
                wr.pages++;
            }
            if (wr.err) { wr.done = r; continue; }
        }
        for (uint32_t o = 0; o < n; o += 256) {   /* read back and compare */
            uint32_t k = n - o > 256 ? 256 : n - o;
            if (rd(a + o, scratch, k) != QSPIFLASH_STATUS_SUCCESS) { wr.err = 95; break; }
            for (uint32_t i = 0; i < k; i++) if (scratch[i] != STG[o + i]) wr.verify_bad++;
        }
        wr.sr1_after = sr1();
        wr.done_addr = a; wr.done_len = n; wr.ticks = DWT->CYCCNT - t0;
        wr.done = r;
    }
}
