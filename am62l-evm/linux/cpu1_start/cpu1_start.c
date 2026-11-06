// SPDX-License-Identifier: GPL-2.0
/* E242: start a Zephyr image on a secondary A53 with a PSCI CPU_ON call. TF-A powers the core and enters `entry` in the non-secure world.
 * Linux must not own the core: boot with maxcpus=1 (the core is then never brought up by the kernel). Load the image into SRAM first (load_cpu1.sh). */
#include <linux/module.h>
#include <linux/arm-smccc.h>
#include <linux/psci.h>

static ulong entry = 0x707F10BC;	/* image D (build-sram-core1-el1) ELF entry */
static uint cpu = 1;
module_param(entry, ulong, 0444);
MODULE_PARM_DESC(entry, "physical entry address of the image on the target core");
module_param(cpu, uint, 0444);
MODULE_PARM_DESC(cpu, "core to start; MPIDR Aff0 = cpu, Aff1 = 0 (AM62L has one cluster of up to four A53 cores)");

static int __init cpu1_start_init(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(PSCI_0_2_FN64_CPU_ON, cpu, entry, 0, 0, 0, 0, 0, &res);
	pr_info("cpu1_start: PSCI CPU_ON mpidr=%#x entry=%#lx -> %ld (0 = success, -4 = already on, -9 = bad address, -3 = denied)\n",
		cpu, entry, (long)res.a0);
	return 0;	/* stay loaded; rmmod is harmless */
}

static void __exit cpu1_start_exit(void) { }

module_init(cpu1_start_init);
module_exit(cpu1_start_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PSCI CPU_ON for a second OS on an unused A53 core");
