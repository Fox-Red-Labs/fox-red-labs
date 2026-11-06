/* Fox Red Labs: Zephyr on R5FSS0-1 beside TI's FreeRTOS on R5FSS0-0. With no console and no system timer on this
 * port, it proves it is running by writing a counter and a line of text into the shared, uncached RAM at
 * 0x701D0800, which the program on core 0 prints. */
#include <zephyr/kernel.h>
#include <zephyr/cache.h>
#include <string.h>
#include <zephyr/version.h>

struct frl_mix {
	volatile uint32_t magic;     /* 0x5A455048 "ZEPH" once running */
	volatile uint32_t counter;   /* incremented about every 10 ms of busy work */
	char text[56];
};
#define FRL_MIX ((struct frl_mix *)0x701D0800u)

int main(void)
{
	static const char msg[] = "Zephyr " KERNEL_VERSION_STRING " on R5FSS0-1";
	FRL_MIX->counter = 0;
	memcpy(FRL_MIX->text, msg, sizeof(msg));
	FRL_MIX->magic = 0x5A455048u;
	sys_cache_data_flush_range((void *)FRL_MIX, sizeof(*FRL_MIX));
	for (;;) {
		for (volatile uint32_t i = 0; i < 1000000u; i++) {
		}
		FRL_MIX->counter++;
		sys_cache_data_flush_range((void *)&FRL_MIX->counter, sizeof(uint32_t));
	}
	return 0;
}
