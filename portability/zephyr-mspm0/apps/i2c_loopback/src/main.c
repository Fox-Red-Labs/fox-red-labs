/*
 * MSPM0 I2C controller and target against each other inside one peripheral (loopback bit).
 * The target is Zephyr's I2C EEPROM target at 0x54 (256 bytes, 8 bit address).
 * Prints "I2C-LOOPBACK: PASS" or "I2C-LOOPBACK: FAIL n".
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c/target/eeprom.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#define I2C_NODE DT_NODELABEL(i2c2)
#define EEPROM_NODE DT_NODELABEL(eeprom0)
#define EEPROM_ADDR 0x54
#define I2C_REG_CCR 0x1228 /* controller configuration */
#define I2C_CCR_LPBK 8      /* bit 8: loopback */

static int fails;

static void check(const char *what, bool ok, int detail)
{
	printk("RESULT %-48s %s (%d)\n", what, ok ? "ok" : "FAIL", detail);
	if (!ok) {
		fails++;
	}
}

static void dump(const char *when)
{
	mem_addr_t b = DT_REG_ADDR(I2C_NODE);

	printk("regs %s: CCR %08x CSR %08x TOAR %08x TCTR %08x\n", when, sys_read32(b + 0x1228),
	       sys_read32(b + 0x1218), sys_read32(b + 0x1250), sys_read32(b + 0x1258));
}

int main(void)
{
	const struct device *i2c = DEVICE_DT_GET(I2C_NODE);
	const struct device *ee = DEVICE_DT_GET(EEPROM_NODE);
	uint8_t pre[16], wr[17], rd[16];
	uint8_t off;
	int rc;

	printk("\nMSPM0 I2C loopback, board %s\n", CONFIG_BOARD_TARGET);
	check("i2c2 ready", device_is_ready(i2c), 0);
	check("eeprom target ready", device_is_ready(ee), 0);
	if (fails) {
		printk("I2C-LOOPBACK: FAIL %d\n", fails);
		return 0;
	}

	rc = i2c_target_driver_register(ee);
	check("i2c_target_driver_register(eeprom 0x54)", rc == 0, rc);
	dump("after target register");
	sys_set_bit(DT_REG_ADDR(I2C_NODE) + I2C_REG_CCR, I2C_CCR_LPBK);
	dump("after LPBK set");

	/* target side preloaded, controller reads it */
	for (int i = 0; i < 16; i++) {
		pre[i] = 0xa0 + i;
	}
	rc = eeprom_target_program(ee, pre, sizeof(pre));
	check("eeprom_target_program", rc == 0, rc);
	off = 0;
	memset(rd, 0, sizeof(rd));
	uint32_t t0 = k_cycle_get_32();

	rc = i2c_write_read(i2c, EEPROM_ADDR, &off, 1, rd, sizeof(rd));
	printk("write-read 1+16 bytes: %u us\n", k_cyc_to_us_floor32(k_cycle_get_32() - t0));
	dump("after first transfer");
	check("controller reads 16 bytes the target holds", rc == 0 && memcmp(rd, pre, 16) == 0, rc);
	printk("read: ");
	for (int i = 0; i < 16; i++) {
		printk("%02x ", rd[i]);
	}
	printk("\n");

	/* controller writes, controller reads back */
	wr[0] = 0x40; /* eeprom offset */
	for (int i = 0; i < 16; i++) {
		wr[1 + i] = 0x31 + 3 * i;
	}
	rc = i2c_write(i2c, wr, sizeof(wr), EEPROM_ADDR);
	check("controller writes 16 bytes at offset 0x40", rc == 0, rc);
	off = 0x40;
	memset(rd, 0, sizeof(rd));
	rc = i2c_write_read(i2c, EEPROM_ADDR, &off, 1, rd, sizeof(rd));
	check("controller reads them back", rc == 0 && memcmp(rd, wr + 1, 16) == 0, rc);

	/* single byte, burst API, and a different offset */
	rc = i2c_reg_write_byte(i2c, EEPROM_ADDR, 0x7f, 0x5a);
	check("i2c_reg_write_byte", rc == 0, rc);
	uint8_t b = 0;

	rc = i2c_reg_read_byte(i2c, EEPROM_ADDR, 0x7f, &b);
	check("i2c_reg_read_byte returns 0x5a", rc == 0 && b == 0x5a, rc);

	/* a 255 byte burst, as large as the controller FIFO path will see */
	uint8_t big[1 + 128], bigrd[128];

	big[0] = 0x00;
	for (int i = 0; i < 128; i++) {
		big[1 + i] = (uint8_t)(i * 5 + 1);
	}
	rc = i2c_write(i2c, big, sizeof(big), EEPROM_ADDR);
	check("controller writes 128 bytes", rc == 0, rc);
	off = 0;
	rc = i2c_write_read(i2c, EEPROM_ADDR, &off, 1, bigrd, sizeof(bigrd));
	check("controller reads 128 bytes back", rc == 0 && memcmp(bigrd, big + 1, 128) == 0, rc);

	/* an address nobody answers */
	rc = i2c_reg_read_byte(i2c, 0x2a, 0, &b);
	check("unused address 0x2a gives an error", rc != 0, rc);

	printk("I2C-LOOPBACK: %s\n", fails ? "FAIL" : "PASS");
	while (1) {
		k_sleep(K_SECONDS(5));
		printk("I2C-LOOPBACK: %s (idle)\n", fails ? "FAIL" : "PASS");
	}
	return 0;
}
