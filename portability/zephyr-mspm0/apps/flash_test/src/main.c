/*
 * MSPM0 internal flash test: erase, write, read back, erase the last sector of the
 * zephyr,flash bank. In the style of samples/drivers/soc_flash_nrf.
 * Prints one "RESULT" line per check and "FLASH-TEST: PASS" or "FLASH-TEST: FAIL n".
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#define FLASH_DEV DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller))
#define FLASH_NODE DT_CHOSEN(zephyr_flash)
#define FLASH_SIZE DT_REG_SIZE(FLASH_NODE)
#define SECTOR DT_PROP(FLASH_NODE, erase_block_size)
#define TEST_OFFSET (FLASH_SIZE - SECTOR) /* last sector of the bank */
#define WORD DT_PROP(FLASH_NODE, write_block_size)

static int fails;

static void check(const char *what, bool ok, int detail)
{
	printk("RESULT %-44s %s (%d)\n", what, ok ? "ok" : "FAIL", detail);
	if (!ok) {
		fails++;
	}
}

static bool all_ff(const uint8_t *p, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		if (p[i] != 0xff) {
			return false;
		}
	}
	return true;
}

static uint8_t buf[SECTOR] __aligned(4);
static uint8_t pat[SECTOR] __aligned(4);

static uint32_t us_since(uint32_t t0)
{
	return k_cyc_to_us_floor32(k_cycle_get_32() - t0);
}

int main(void)
{
	const struct device *dev = FLASH_DEV;
	const struct flash_parameters *fp;
	struct flash_pages_info pi;
	uint64_t size = 0;
	uint32_t t0;
	int rc;

	printk("\nMSPM0 flash test, board %s\n", CONFIG_BOARD_TARGET);
	check("device ready", device_is_ready(dev), 0);
	if (!device_is_ready(dev)) {
		printk("FLASH-TEST: FAIL %d\n", fails);
		return 0;
	}

	fp = flash_get_parameters(dev);
	printk("write_block_size %u, erase_value 0x%02x, zephyr,flash size %u, sector %u, offset under test 0x%x\n",
	       fp->write_block_size, fp->erase_value, (unsigned int)FLASH_SIZE, (unsigned int)SECTOR,
	       (unsigned int)TEST_OFFSET);
	check("write_block_size is 8", fp->write_block_size == 8, fp->write_block_size);
	rc = flash_get_page_info_by_offs(dev, TEST_OFFSET, &pi);
	check("page info of last sector", rc == 0 && pi.size == SECTOR && pi.start_offset == TEST_OFFSET, rc);
	rc = flash_get_size(dev, &size);
	printk("flash_get_size rc %d size %u\n", rc, (unsigned int)size);
	check("flash_get_size equals the devicetree size", rc == 0 && size == FLASH_SIZE, rc);

	for (size_t i = 0; i < SECTOR; i++) {
		pat[i] = (uint8_t)(i * 7 + 3);
	}

	/* 1. erase */
	t0 = k_cycle_get_32();
	rc = flash_erase(dev, TEST_OFFSET, SECTOR);
	printk("erase %u bytes: %u us\n", (unsigned int)SECTOR, us_since(t0));
	check("erase last sector", rc == 0, rc);
	rc = flash_read(dev, TEST_OFFSET, buf, SECTOR);
	check("sector reads 0xff after erase", rc == 0 && all_ff(buf, SECTOR), rc);

	/* 2. write the whole sector */
	t0 = k_cycle_get_32();
	rc = flash_write(dev, TEST_OFFSET, pat, SECTOR);
	printk("write %u bytes: %u us\n", (unsigned int)SECTOR, us_since(t0));
	check("write full sector", rc == 0, rc);
	memset(buf, 0, SECTOR);
	rc = flash_read(dev, TEST_OFFSET, buf, SECTOR);
	check("read back equals pattern", rc == 0 && memcmp(buf, pat, SECTOR) == 0, rc);
	check("direct memory read equals pattern",
	      memcmp((const void *)(DT_REG_ADDR(FLASH_NODE) + TEST_OFFSET), pat, SECTOR) == 0, 0);

	/* 3. argument checks */
	rc = flash_write(dev, TEST_OFFSET + 4, pat, WORD);
	check("write at unaligned offset is refused", rc == -EINVAL, rc);
	rc = flash_write(dev, TEST_OFFSET, pat, WORD + 1);
	check("write of unaligned length is refused", rc == -EINVAL, rc);
	rc = flash_erase(dev, TEST_OFFSET + 8, SECTOR);
	check("erase at unaligned offset is refused", rc == -EINVAL, rc);
	rc = flash_read(dev, FLASH_SIZE, buf, 1);
	check("read past the end is refused", rc == -EINVAL, rc);

	/* 4. a source buffer that is not word aligned */
	t0 = k_cycle_get_32();
	rc = flash_erase(dev, TEST_OFFSET, SECTOR);
	printk("erase of a programmed sector: %u us\n", us_since(t0));
	check("erase a programmed sector", rc == 0, rc);
	rc = flash_write(dev, TEST_OFFSET, pat + 1, 16);
	check("write from an unaligned source", rc == 0, rc);
	memset(buf, 0, 16);
	rc = flash_read(dev, TEST_OFFSET, buf, 16);
	check("unaligned-source data reads back", rc == 0 && memcmp(buf, pat + 1, 16) == 0, rc);

	/* 5. write over a word that is not erased: information only, the hardware decides */
	rc = flash_write(dev, TEST_OFFSET, pat + 16, 8);
	printk("INFO write over a programmed word returned %d\n", rc);

#ifdef CONFIG_APP_LEAVE_PATTERN
	/* Leave the pattern programmed so the host can read it with the debugger. */
	rc = flash_erase(dev, TEST_OFFSET, SECTOR);
	check("erase before leaving the pattern", rc == 0, rc);
	rc = flash_write(dev, TEST_OFFSET, pat, SECTOR);
	check("write the pattern to leave", rc == 0, rc);
	printk("PATTERN LEFT at 0x%x, %u bytes, byte i = (i*7+3) & 0xff\n", (unsigned int)TEST_OFFSET,
	       (unsigned int)SECTOR);
	printk("FLASH-TEST: %s\n", fails ? "FAIL" : "PASS");
	while (1) {
		k_sleep(K_SECONDS(5));
	}
#endif

	/* 6. erase leaves the sector blank */
	rc = flash_erase(dev, TEST_OFFSET, SECTOR);
	check("final erase", rc == 0, rc);
	rc = flash_read(dev, TEST_OFFSET, buf, SECTOR);
	check("sector reads 0xff after final erase", rc == 0 && all_ff(buf, SECTOR), rc);

	printk("FLASH-TEST: %s\n", fails ? "FAIL" : "PASS");
	if (fails) {
		printk("failed checks: %d\n", fails);
	}
	while (1) {
		k_sleep(K_SECONDS(5));
		printk("FLASH-TEST: %s (idle)\n", fails ? "FAIL" : "PASS");
	}
	return 0;
}
