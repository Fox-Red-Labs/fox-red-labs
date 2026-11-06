/* printk to a ring in on-chip SRAM, readable by Linux on the other core (am62l-evm/tools/ringcat.py).
 * Layout at CONFIG_FRL_RING_ADDR: magic, head (total bytes written), size, reserved, then 4080 data bytes.
 * The page is mapped Normal non-cacheable by core1-ring-map.overlay (the MMU maps nothing outside the image otherwise; Renode showed the data
 * abort). Non-cacheable matters: Linux reads this SRAM through an uncached /dev/mem mapping, which does not snoop this core's L1, so a cached
 * ring would stay invisible until evicted. A dsb after each byte orders the data before the head index. */
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/sys/printk.h>
#include <stdint.h>

#define RING_BYTES 4096U
#define RING_HDR 16U
#define RING_DATA (RING_BYTES - RING_HDR)
#define RING_MAGIC 0x52524C46U

struct frl_ring {
	volatile uint32_t magic;
	volatile uint32_t head;
	volatile uint32_t size;
	volatile uint32_t reserved;
	volatile uint8_t data[RING_DATA];
};

static struct frl_ring *const ring = (struct frl_ring *)CONFIG_FRL_RING_ADDR;

static int ring_out(int c)
{
	uint32_t h = ring->head;

	ring->data[h % RING_DATA] = (uint8_t)c;
	__asm__ volatile("dsb sy" ::: "memory");
	ring->head = h + 1U;
	__asm__ volatile("dsb sy" ::: "memory");
	return c;
}

static int ring_init(void)
{
	ring->head = 0U;
	ring->size = RING_DATA;
	ring->reserved = 0U;
	__asm__ volatile("dsb sy" ::: "memory");
	ring->magic = RING_MAGIC;
	__asm__ volatile("dsb sy" ::: "memory");
	__printk_hook_install(ring_out);
	return 0;
}
SYS_INIT(ring_init, PRE_KERNEL_1, 0);
