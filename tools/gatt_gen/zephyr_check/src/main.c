#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include "blinky_service.h"
#include "bench_probe_service.h"

static void changed(struct bt_conn *conn, uint8_t char_id, uint16_t len, const uint8_t *value)
{
	if (char_id == BLINKY_LED_ID && len == BLINKY_LED_LEN) {
		/* drive the LED from value[0] here */
	}
}

int main(void)
{
	uint8_t pressed = 1, cmd[BENCH_PROBE_COMMAND_LEN] = {0};
	uint16_t n = sizeof(cmd);

	bt_enable(NULL);
	blinky_service_init(changed);
	bench_probe_service_init(NULL);
	blinky_service_set_button(&pressed, sizeof(pressed));
	blinky_service_notify_button();
	bench_probe_service_get_command(cmd, &n);
	bench_probe_service_notify_reading();
	bench_probe_service_indicate_reading();
	bench_probe_service_indicate_alarm();
	return 0;
}

/* Link check only (this is not a usable random source): with host crypto off, the host expects the controller's random numbers. Not random. */
int bt_rand(void *buf, size_t len)
{
	memset(buf, 0x5a, len);
	return 0;
}
