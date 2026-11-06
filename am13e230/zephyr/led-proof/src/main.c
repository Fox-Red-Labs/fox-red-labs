/* LED proof for upstream Zephyr on the LP-AM13E230, which has no GPIO or UART driver yet.
 * Toggles LED4 (red, PA5, active low) every 250 ms by writing the registers named in the AM13E2x SDK
 * (hw_gpio.h, hw_iomux.h, hw_pinmap.h). Not TI code: register addresses only.
 * `proof_count` counts the toggles, so a debugger can prove the kernel is running: read it twice. */
#include <zephyr/kernel.h>
#include <zephyr/sys/sys_io.h>

#define IOMUX_PINCM5   0x400CC000UL + 5 * 4   /* PA5 pin control */
#define GPIO0_BASE     0x400F0000UL           /* port A */
#define GPIO_PWREN     (GPIO0_BASE + 0x800)
#define GPIO_DOUTTGL   (GPIO0_BASE + 0x12B0)
#define GPIO_DOUTSET   (GPIO0_BASE + 0x1290)
#define GPIO_DOE       (GPIO0_BASE + 0x12C0)

volatile uint32_t proof_count;

int main(void)
{
	sys_write32(0x26000000U | 1U, GPIO_PWREN);   /* key + enable */
	sys_write32(0x80U | 1U, IOMUX_PINCM5);       /* connected, GPIO function */
	sys_write32(1U << 5, GPIO_DOUTSET);          /* LED off (active low) */
	sys_write32(1U << 5, GPIO_DOE);              /* output enable */
	for (;;) {
		sys_write32(1U << 5, GPIO_DOUTTGL);
		proof_count++;
		k_msleep(250);
	}
	return 0;
}
