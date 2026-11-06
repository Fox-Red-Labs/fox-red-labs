/* E242: Zephyr on A53_1 in on-chip SRAM beside Linux on A53_0. Prints which core and exception level it runs on,
 * then one line a second for ever. k_busy_wait only: no dependence on timer interrupts reaching this core. */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdint.h>

int main(void)
{
	uint64_t mpidr, el;
	uint32_t n = 0;

	__asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
	__asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
	printk("core1-hello: MPIDR_EL1=0x%llx (Aff0=%llu) CurrentEL=EL%llu\n",
	       (unsigned long long)mpidr, (unsigned long long)(mpidr & 0xff), (unsigned long long)(el >> 2));
	for (;;) {
		printk("core1-hello: tick %u\n", n++);
		k_busy_wait(1000000);
	}
	return 0;
}
