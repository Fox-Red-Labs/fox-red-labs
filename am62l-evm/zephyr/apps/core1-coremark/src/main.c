/* CoreMark on the Zephyr core (core 1). Prints to the console ring. */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
int coremark_main(int argc, char *argv[]);
int main(void)
{
	uint64_t f, mp, el;
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
	__asm__ volatile("mrs %0, mpidr_el1" : "=r"(mp));
	__asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
	printk("coremark: MPIDR=0x%llx EL%llu cntfrq=%llu\n", (unsigned long long)mp, (unsigned long long)(el >> 2), (unsigned long long)f);
	char *argv[] = {"coremark", NULL};
	for (unsigned n = 0;; n++) {   /* repeat so a Linux run can overlap in time; the ring keeps the last reports */
		printk("coremark: pass %u\n", n);
		coremark_main(1, argv);
	}
	return 0;
}
