/*
 * MSPM0 I2C controller check on whatever sits on the LP-MSPM0G3519's BoosterPack I2C pins
 * (I2C2 on PC2/PC3): bus scan, then ID registers of the devices that answer.
 * In the style of samples/drivers/i2c/scan. Prints "I2C-PROBE: PASS" or "I2C-PROBE: FAIL".
 *   FT6336U touch controller 0x38 (Waveshare 3.5 inch panel): reg 0xA8 vendor 0x11, reg 0xA3 chip 0x64
 *   HDC2080 0x40 (BP-BASSENSORSMKII): regs 0xFC/0xFD = 0x49 0x54 ("TI"), 0xFE/0xFF = 0xD0 0x07;
 *     temperature: trigger MEAS_CONFIG 0x0F, read 0x00..0x03: T = raw / 65536 * 165 - 40.5
 *   OPT3001 0x44: reg 0x7E = 0x5449, reg 0x7F = 0x3001 (16 bit, big endian)
 * A device is checked only if it answers the scan. PASS needs one known device with the right ID
 * and no known device with a wrong one.
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define I2C_NODE DT_NODELABEL(i2c2)

static int fails;
static int good;

static void check(const char *what, bool ok, int detail)
{
	printk("RESULT %-48s %s (%d)\n", what, ok ? "ok" : "FAIL", detail);
	if (ok) {
		good++;
	} else {
		fails++;
	}
}

#ifdef BS_POWER
static const struct gpio_dt_spec hdc = GPIO_DT_SPEC_GET(DT_NODELABEL(bs_hdc), gpios);
static const struct gpio_dt_spec drv = GPIO_DT_SPEC_GET(DT_NODELABEL(bs_drv), gpios);
static const struct gpio_dt_spec opt = GPIO_DT_SPEC_GET(DT_NODELABEL(bs_opt), gpios);
static const struct gpio_dt_spec tmp = GPIO_DT_SPEC_GET(DT_NODELABEL(bs_tmp), gpios);
#endif

static bool present[128];

int main(void)
{
	const struct device *i2c = DEVICE_DT_GET(I2C_NODE);
	uint8_t r[2];
	int rc, found = 0;
	uint32_t t0;

	printk("\nMSPM0 I2C probe, board %s\n", CONFIG_BOARD_TARGET);
	if (!device_is_ready(i2c)) {
		check("i2c device ready", false, 0);
		printk("I2C-PROBE: FAIL\n");
		return 0;
	}

#ifdef BS_POWER
	/*
	 * BP-BASSENSORSMKII power pins: HDC2080 and DRV5055 low = on, OPT3001 and TMP117 high = on.
	 * Build with -DBS_POWER=1 only with that pack stacked: with the Waveshare panel wired, PA25 and
	 * PB26 are display pins and must not be driven.
	 */
	gpio_pin_configure_dt(&hdc, GPIO_OUTPUT_LOW);
	gpio_pin_configure_dt(&drv, GPIO_OUTPUT_LOW);
	gpio_pin_configure_dt(&opt, GPIO_OUTPUT_HIGH);
	gpio_pin_configure_dt(&tmp, GPIO_OUTPUT_HIGH);
	k_msleep(50);
#endif

	printk("scan (7-bit addresses, one-byte read each):\n");
	for (uint8_t a = 0x08; a <= 0x77; a++) {
		uint8_t d = 0xee;

		t0 = k_cycle_get_32();
		rc = i2c_read(i2c, &d, 1, a);
		if (rc == 0) {
			printk("  0x%02x answers (%u us), data byte 0x%02x\n", a,
			       k_cyc_to_us_floor32(k_cycle_get_32() - t0), d);
			present[a] = true;
			found++;
		}
	}
	printk("scan found %d device(s)\n", found);
	if (found == 0) {
		fails++;
		printk("RESULT scan finds at least one device                  FAIL (0)\n");
	}

	if (present[0x38]) {
		uint8_t v = 0, c = 0, fw = 0, tp = 0xff;

		rc = i2c_reg_read_byte(i2c, 0x38, 0xa8, &v);
		int rc2 = i2c_reg_read_byte(i2c, 0x38, 0xa3, &c);

		i2c_reg_read_byte(i2c, 0x38, 0xa6, &fw);
		i2c_reg_read_byte(i2c, 0x38, 0x02, &tp);
		printk("FT6336U vendor 0x%02x chip 0x%02x firmware 0x%02x touch points %u (rc %d %d)\n", v,
		       c, fw, tp, rc, rc2);
		check("FT6336U vendor ID 0x11 and chip ID 0x64", rc == 0 && rc2 == 0 && v == 0x11 && c == 0x64,
		      rc ? rc : rc2);
	} else {
		printk("FT6336U 0x38: not on the bus\n");
	}

	if (present[0x40]) {
		rc = i2c_burst_read(i2c, 0x40, 0xfc, r, 2);
		printk("HDC2080 manufacturer id bytes: rc %d  %02x %02x\n", rc, r[0], r[1]);
		check("HDC2080 manufacturer ID 0x5449", rc == 0 && r[0] == 0x49 && r[1] == 0x54, rc);
		rc = i2c_burst_read(i2c, 0x40, 0xfe, r, 2);
		printk("HDC2080 device id bytes: rc %d  %02x %02x\n", rc, r[0], r[1]);
		check("HDC2080 device ID 0x07D0", rc == 0 && r[0] == 0xd0 && r[1] == 0x07, rc);

		uint8_t cmd[2] = {0x0f, 0x01};
		uint8_t m[4];

		rc = i2c_write(i2c, cmd, 2, 0x40);
		k_msleep(30);
		rc |= i2c_burst_read(i2c, 0x40, 0x00, m, 4);
		if (rc == 0) {
			uint32_t t_raw = m[0] | (m[1] << 8);
			uint32_t h_raw = m[2] | (m[3] << 8);
			int32_t t_mc = (int32_t)((t_raw * 165000ULL) / 65536) - 40500;
			int32_t h_pm = (int32_t)((h_raw * 1000ULL) / 65536);

			printk("HDC2080 raw %04x %04x -> %d.%02d C, %d.%d %%RH\n", (unsigned int)t_raw,
			       (unsigned int)h_raw, t_mc / 1000, (t_mc < 0 ? -t_mc : t_mc) % 1000 / 10,
			       h_pm / 10, h_pm % 10);
			check("HDC2080 temperature between 10 and 45 C", t_mc > 10000 && t_mc < 45000, t_mc);
		} else {
			check("HDC2080 temperature read", false, rc);
		}
	} else {
		printk("HDC2080 0x40: not on the bus (BP-BASSENSORSMKII not stacked)\n");
	}

	if (present[0x44]) {
		rc = i2c_burst_read(i2c, 0x44, 0x7e, r, 2);
		printk("OPT3001 manufacturer id: rc %d  %02x%02x\n", rc, r[0], r[1]);
		check("OPT3001 manufacturer ID 0x5449", rc == 0 && r[0] == 0x54 && r[1] == 0x49, rc);
		rc = i2c_burst_read(i2c, 0x44, 0x7f, r, 2);
		printk("OPT3001 device id: rc %d  %02x%02x\n", rc, r[0], r[1]);
		check("OPT3001 device ID 0x3001", rc == 0 && r[0] == 0x30 && r[1] == 0x01, rc);
	} else {
		printk("OPT3001 0x44: not on the bus\n");
	}

	/* write, repeated start, read through the transfer array, on the first device found */
	for (int a = 0x08; a <= 0x77; a++) {
		if (present[a] && a == 0x38) {
			struct i2c_msg msgs[2];
			uint8_t reg = 0xa3, id = 0;

			msgs[0].buf = &reg;
			msgs[0].len = 1;
			msgs[0].flags = I2C_MSG_WRITE;
			msgs[1].buf = &id;
			msgs[1].len = 1;
			msgs[1].flags = I2C_MSG_READ | I2C_MSG_STOP;
			rc = i2c_transfer(i2c, msgs, 2, a);
			check("i2c_transfer write + repeated start + read (0xA3)", rc == 0 && id == 0x64, rc);
		}
	}

	/* an address nobody has must give an error, not hang */
	t0 = k_cycle_get_32();
	rc = i2c_read(i2c, r, 1, 0x7a);
	printk("read from empty address 0x7a: rc %d after %u us\n", rc,
	       k_cyc_to_us_floor32(k_cycle_get_32() - t0));
	check("empty address gives an error", rc != 0, rc);

	bool pass = fails == 0 && good >= 2;

	printk("I2C-PROBE: %s\n", pass ? "PASS" : "FAIL");
	while (1) {
		k_sleep(K_SECONDS(5));
		printk("I2C-PROBE: %s (idle)\n", pass ? "PASS" : "FAIL");
	}
	return 0;
}
