/* E199 baud probe: stream 'U' (0x55, alternating bits) on UART0 forever.
 * A receiver at any rate gets a steady byte count per second; the count that
 * comes out as clean 0x55 is the real bit rate of this UART without TIFS. */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

int main(void)
{
	const struct device *u = DEVICE_DT_GET(DT_NODELABEL(main_uart0));
	if (!device_is_ready(u)) {
		return 0;
	}
	for (;;) {
		uart_poll_out(u, 'U');
	}
	return 0;
}
