/* LP-MSPM33C321A platform glue for the m33-whx Doom engine (E27 step 6). GPL-2.0 with the engine it is linked with.
 * - fox_board_init: SysConfig set-up (160 MHz, QSPI pins), SDK QSPI driver, DWT cycle counter.
 * - fox_board_qspi_read: the page cache's miss path. Quad I/O read (command 0xEB, 1-4-4, 6 dummy clocks) at 40 MHz through the SDK's
 *   QSPIFLASH_read, polled; the quad-enable bit of the flash is already set, so nothing is written to the flash. Only reads reach the
 *   chip: 0xEB here, nothing else after init. The file sits at WHX_FLASH_BASE, a blank region found by the dump (E27 step 4).
 * - fox_board_ms: a clock that advances 35 ms per read (see platform.c), so the demos play as in the emulator.
 * - fox_board_frame / fox_board_demo_end: frame checksums and timing, kept in the RAM struct fb_res for the host to read.
 * Written fresh from the SDK driver headers (source/ti/external_flash/QSPI_FLASH.h) and the DWT register description. */
#include <stdint.h>
#include <string.h>
#include <ti/external_flash/QSPI_FLASH.h>
#include "ti_msp_dl_config.h"

#ifndef WHX_FLASH_BASE
#define WHX_FLASH_BASE 0x00640000u
#endif
#ifndef WHX_BYTES
#define WHX_BYTES 1800312u
#endif
#ifndef WHX_CRC32
#define WHX_CRC32 0u
#endif

typedef struct {
    uint32_t magic;            /* 0xD00D0C01 */
    uint32_t state;            /* 1 init, 2 self-check done, 3 running, 4 done (3 demos), 9 fault */
    uint32_t fault;            /* 1 read failed, 2 self-check mismatch */
    uint32_t whx_base, whx_bytes, whx_crc_expect, whx_crc_got, selfcheck_ms10, selfcheck_reads;
    uint32_t frames, demos_done;
    uint32_t chain_crc;        /* CRC-32 chained over every frame buffer (320 x 168 bytes), the same figure as the emulator's framecrc */
    uint32_t demo_chain[3], demo_frames[3], demo_gametic[3];
    uint64_t demo_cyc[3];      /* CPU cycles in the frame loop, frame-checksum time left out */
    uint64_t demo_qcyc[3];     /* of which inside qspi_read */
    uint32_t demo_qreads[3], demo_qbytes[3];
    uint32_t demo_fast[3], demo_slow[3], demo_set[3], demo_miss[3];   /* cache counters when built with WHXSTATS=1 */
    uint32_t demo_fmax[3], demo_qmax[3];   /* longest frame, and most QSPI cycles in one frame */
    uint32_t init_cyc;         /* cycles from reset to the first frame */
    uint32_t crc_cyc_total_hi, crc_cyc_total_lo;
    uint32_t cpu_hz_assumed;
    uint32_t log_n;
    char log[96];
} fb_res_t;
volatile fb_res_t fb_res;

extern uint32_t gametic;
extern uint32_t whx_st_fast, whx_st_set, whx_st_miss, whx_st_slow;

#ifdef FOX_EDU
void edu_init(void);
void edu_text(int x, int y, const char *s);
#endif
static QSPIFLASH_Handle qh;
static QSPIFLASH_InitParams qp;
static uint64_t q_cyc, q_cyc_mark; static uint32_t q_reads, q_bytes, q_reads_mark, q_bytes_mark;
static uint32_t fake_ms;

#ifndef FOX_MS_PER_READ
#define FOX_MS_PER_READ 35
#endif
#ifdef FOX_RT
/* Real time (the display images): milliseconds from the DWT cycle counter, so the game runs at its own 35 tics a second and a slow frame skips tics. */
uint32_t fox_board_ms(void)
{
    static uint32_t last, rem; uint32_t c = DWT->CYCCNT, d = (c - last) + rem; last = c;
    fake_ms += d / (CPUCLK_FREQ / 1000u); rem = d % (CPUCLK_FREQ / 1000u); return fake_ms;
}
#else
uint32_t fox_board_ms(void) { fake_ms += FOX_MS_PER_READ; return fake_ms; }
#endif
void fox_board_putc(char c) { if (fb_res.log_n < sizeof fb_res.log - 1) fb_res.log[fb_res.log_n++] = c; }

static inline uint32_t cyc(void) { return DWT->CYCCNT; }
static void fault(uint32_t code) { fb_res.fault = code; fb_res.state = 9; DL_GPIO_setPins(GPIO_LEDS_PORT, GPIO_LEDS_FAIL_LED_PIN); for (;;) {} }

void fox_board_qspi_read(uint32_t off, void *dst, uint32_t n)
{
    uint32_t t0 = cyc();
    QSPIFLASH_CommandConfig c = {0};
    c.instruction  = QUAD_IO_READ_CMD;
    c.address      = WHX_FLASH_BASE + off;
    c.addressSize  = QSPIFLASH_ADDRESS_SIZE_3BYTE;
    c.dummyCycles  = DUMMY_CYCLES_QUAD_IO_READ;
    c.transferMode = QSPIFLASH_TRANSFER_MODE_QUAD_IO;
    if (QSPIFLASH_read(&qh, dst, n, &c) != QSPIFLASH_STATUS_SUCCESS) fault(1);
    q_cyc += (uint32_t)(cyc() - t0); q_reads++; q_bytes += n;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *p, uint32_t n)   /* standard CRC-32 (zlib), 4 bits a step, 64-byte table */
{
    static const uint32_t T[16] = {0x00000000,0x1DB71064,0x3B6E20C8,0x26D930AC,0x76DC4190,0x6B6B51F4,0x4DB26158,0x5005713C,
                                   0xEDB88320,0xF00F9344,0xD6D6A3E8,0xCB61B38C,0x9B64C2B0,0x86D3D2D4,0xA00AE278,0xBDBDF21C};
    crc = ~crc;
    while (n--) { crc ^= *p++; crc = (crc >> 4) ^ T[crc & 15]; crc = (crc >> 4) ^ T[crc & 15]; }
    return ~crc;
}

void fox_board_init(void)
{
    SYSCFG_DL_init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
#ifdef FOX_EDU
    edu_init(); edu_text(8, 56, "DOOM");  edu_text(8, 68, "checking the data file");
#endif
    fb_res.magic = 0xD00D0C01u; fb_res.state = 1; fb_res.cpu_hz_assumed = 160000000u;
    fb_res.whx_base = WHX_FLASH_BASE; fb_res.whx_bytes = WHX_BYTES; fb_res.whx_crc_expect = WHX_CRC32;
    qp.qspiRegs = QSPI;
    if (QSPIFLASH_init(&qh, &qp) != QSPIFLASH_STATUS_SUCCESS) fault(1);
    /* Self-check: read the whole file through the same path the cache uses (256-byte reads) and compare its CRC-32 with the host's. */
    static uint8_t page[256] __attribute__((aligned(4)));
    uint32_t crc = 0, t0 = cyc(), r0 = q_reads;
    for (uint32_t o = 0; o < WHX_BYTES; o += 256) {
        uint32_t k = WHX_BYTES - o; if (k > 256) k = 256;
        fox_board_qspi_read(o, page, k); crc = crc32_update(crc, page, k);
    }
    fb_res.whx_crc_got = crc; fb_res.selfcheck_ms10 = (uint32_t)(((uint64_t)(cyc() - t0) * 10u) / 160000u); fb_res.selfcheck_reads = q_reads - r0;
    q_cyc = 0; q_reads = 0; q_bytes = 0;
    if (WHX_CRC32 && crc != WHX_CRC32) fault(2);
    fb_res.state = 3;
    DWT->CYCCNT = 0;
}

static uint32_t last_t, crc_cyc_hi, crc_cyc_lo; static int have_last;
static uint64_t run_cyc, run_qcyc;   /* totals since the demo started */
static uint32_t fmax, qmax;

void fox_board_frame(const uint8_t *fb)
{
    uint32_t t0 = cyc();
    if (have_last) {
        uint32_t d = t0 - last_t; uint32_t q = (uint32_t)(q_cyc - q_cyc_mark);
        run_cyc += d; run_qcyc += q; if (d > fmax) fmax = d; if (q > qmax) qmax = q;
    } else fb_res.init_cyc = t0;
    q_cyc_mark = q_cyc;
#ifndef FOX_NO_FRAMECRC
    fb_res.chain_crc = crc32_update(fb_res.chain_crc, fb, 320u * 168u);
#endif
    fb_res.frames++;
    last_t = cyc(); have_last = 1;
    uint32_t c = last_t - t0; if ((crc_cyc_lo += c) < c) crc_cyc_hi++;
    fb_res.crc_cyc_total_hi = crc_cyc_hi; fb_res.crc_cyc_total_lo = crc_cyc_lo;
}

void fox_board_demo_end(void)   /* called at the entry of G_CheckDemoStatus (build.sh adds the call), as the emulator's hook is */
{
    uint32_t i = fb_res.demos_done;
    if (i < 3) {
        fb_res.demo_chain[i] = fb_res.chain_crc; fb_res.demo_frames[i] = fb_res.frames; fb_res.demo_gametic[i] = gametic;
        fb_res.demo_cyc[i] = run_cyc; fb_res.demo_qcyc[i] = run_qcyc; fb_res.demo_qreads[i] = q_reads; fb_res.demo_qbytes[i] = q_bytes;
        fb_res.demo_fast[i] = whx_st_fast; fb_res.demo_slow[i] = whx_st_slow; fb_res.demo_set[i] = whx_st_set; fb_res.demo_miss[i] = whx_st_miss;
        fb_res.demo_fmax[i] = fmax; fb_res.demo_qmax[i] = qmax;   /* cumulative maxima; the host takes the per-demo ones from the run order */
        fmax = 0; qmax = 0;
        fb_res.demos_done = ++i;
    }
#ifdef FOX_EDU
    if (i >= 3 && fb_res.state != 4) { fb_res.state = 4; DL_GPIO_setPins(GPIO_LEDS_PORT, GPIO_LEDS_PASS_LED_PIN); }   /* the display image keeps playing the demo loop */
#else
    if (i >= 3) { fb_res.state = 4; DL_GPIO_setPins(GPIO_LEDS_PORT, GPIO_LEDS_PASS_LED_PIN); for (;;) {} }
#endif
}
