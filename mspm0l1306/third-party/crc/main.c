/*
 * CRC-32 (IEEE 802.3, the zlib/Ethernet one) over 1024 bytes: software bitwise, software 256-entry
 * table, and the hardware CRC module through DriverLib. Check value for "123456789" is 0xCBF43926.
 * Hardware setup that matches IEEE: 32-bit polynomial (0x04C11DB7 fixed in silicon), bit reversal
 * ON (input and output), seed 0xFFFFFFFF, final XOR 0xFFFFFFFF done in software.
 * L1306 port: the TRNG part of the original crc_trng image is dropped (no TRNG on this chip).
 */
#include "ti_msp_dl_config.h"
#include <ti/driverlib/dl_crc.h>
#include <string.h>
#include "tb.h"
#include "crc32_table.h"

#define LEN 1024U
#define CHECK_VALUE 0xCBF43926u

static uint32_t buf32[LEN / 4];
static uint8_t *const buf = (uint8_t *)buf32;

static uint32_t crc_bitwise(const uint8_t *p, uint32_t n)
{
	uint32_t c = 0xFFFFFFFFu;
	while (n--) {
		c ^= *p++;
		for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0U - (c & 1U)));
	}
	return ~c;
}

static uint32_t crc_table(const uint8_t *p, uint32_t n)
{
	uint32_t c = 0xFFFFFFFFu;
	while (n--) c = (c >> 8) ^ crc32_table[(c ^ *p++) & 0xFFU];
	return ~c;
}

static void hw_crc_cfg(DL_CRC_BIT rev, DL_CRC_OUTPUT_BYTESWAP bs)
{
	DL_CRC_init(CRC, DL_CRC_32_POLYNOMIAL, rev, DL_CRC_INPUT_ENDIANESS_LITTLE_ENDIAN, bs);
	DL_CRC_setSeed32(CRC, 0xFFFFFFFFu);
}

/* one-shot hardware CRC with byte feeds, for the check string and the probe */
static uint32_t hw_crc_bytes(const uint8_t *p, uint32_t n)
{
	hw_crc_cfg(DL_CRC_BIT_REVERSED, DL_CRC_OUTPUT_BYTESWAP_DISABLED);
	while (n--) DL_CRC_feedData8(CRC, *p++);
	return DL_CRC_getResult32(CRC) ^ 0xFFFFFFFFu;
}

/* 32-bit feeds (4 bytes per write). */
static uint32_t hw_crc_words(const uint32_t *p, uint32_t nwords)
{
	hw_crc_cfg(DL_CRC_BIT_REVERSED, DL_CRC_OUTPUT_BYTESWAP_DISABLED);
	while (nwords--) DL_CRC_feedData32(CRC, *p++);
	return DL_CRC_getResult32(CRC) ^ 0xFFFFFFFFu;
}

/* If the standard setup does not give 0xCBF43926, try the other bit-order, byte-swap and final-XOR
 * combinations and report which one does. Result is a bit mask in the note. */
static void hw_crc_probe(tb_note_t *nt)
{
	static const char s[] = "123456789";
	for (int rev = 0; rev < 2; rev++)
		for (int bs = 0; bs < 2; bs++)
			for (int fx = 0; fx < 2; fx++) {
				hw_crc_cfg(rev ? DL_CRC_BIT_REVERSED : DL_CRC_BIT_NOT_REVERSED,
					bs ? DL_CRC_OUTPUT_BYTESWAP_ENABLED : DL_CRC_OUTPUT_BYTESWAP_DISABLED);
				for (int i = 0; i < 9; i++) DL_CRC_feedData8(CRC, (uint8_t)s[i]);
				uint32_t r = DL_CRC_getResult32(CRC) ^ (fx ? 0xFFFFFFFFu : 0U);
				if (r == CHECK_VALUE) {
					tb_ns(nt, "alt_match_rev="); tb_nu(nt, (uint32_t)rev);
					tb_ns(nt, ";byteswap="); tb_nu(nt, (uint32_t)bs);
					tb_ns(nt, ";finalxor="); tb_nu(nt, (uint32_t)fx);
					return;
				}
			}
	tb_ns(nt, "no_config_matched");
}

int main(void)
{
	uint32_t c, r;
	TB_NOTE(nt);

	tb_start("crc");
	DL_CRC_reset(CRC);
	DL_CRC_enablePower(CRC);
	delay_cycles(POWER_STARTUP_DELAY);

	for (uint32_t i = 0; i < LEN; i++) buf[i] = (uint8_t)(i * 13U + 5U);

	/* ---- check values ---- */
	tb_result("crc32", "sw_bitwise_check", 0, crc_bitwise((const uint8_t *)"123456789", 9) == CHECK_VALUE, "123456789");
	tb_result("crc32", "sw_table_check", 0, crc_table((const uint8_t *)"123456789", 9) == CHECK_VALUE, "123456789");
	r = hw_crc_bytes((const uint8_t *)"123456789", 9);
	int hw_ok = (r == CHECK_VALUE);
	tb_ns(&nt, hw_ok ? "rev=1;seed=ffffffff;xorout=ffffffff;byte_feeds" : "std_cfg_gave_0x");
	if (!hw_ok) { tb_nx(&nt, r); tb_ns(&nt, ";"); hw_crc_probe(&nt); }
	tb_result("crc32", "hw_check", 0, hw_ok, nt.b);

	/* ---- 1024 bytes ---- */
	uint32_t ref = crc_bitwise(buf, LEN), v = 0;
	/* Read the pointer through a volatile so the compiler cannot hoist the whole pure
	 * calculation out of the timing loop (the first run measured 45 cycles for 1 KB). */
	const uint8_t *volatile vbuf = buf;
	/* The result also goes to a volatile, so it must exist before the second timer read:
	 * a local result let the compiler finish the work after the timer stopped. */
	static volatile uint32_t sink;
	TB_BEST(c, 5, sink = v = crc_bitwise(vbuf, LEN));
	tb_result("crc32", "sw_bitwise_1k", c, v == ref, "poly_edb88320");
	TB_BEST(c, 5, sink = v = crc_table(vbuf, LEN));
	tb_result("crc32", "sw_table_1k", c, v == ref, "table_in_flash_1KB");
	TB_BEST(c, 5, v = hw_crc_bytes(buf, LEN));
	tb_result("crc32", "hw_bytes_1k", c, v == ref, "feedData8_x1024");
	TB_BEST(c, 5, v = hw_crc_words(buf32, LEN / 4U));
	tb_result("crc32", "hw_words_1k", c, v == ref, "feedData32_x256");
	TB_BEST(c, 5, v = DL_CRC_calculateBlock32(CRC, 0xFFFFFFFFu, buf32, LEN / 4U));
	{
		/* DriverLib's block helper does not touch CRCCTRL: it uses whatever hw_crc_cfg() left (reversed, LE). */
		TB_NOTE(n2);
		tb_ns(&n2, "DL_CRC_calculateBlock32_raw=0x"); tb_nx(&n2, v);
		tb_result("crc32", "hw_dl_block32_1k", c, (v ^ 0xFFFFFFFFu) == ref, n2.b);
	}

	tb_done();
}
