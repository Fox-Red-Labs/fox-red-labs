/* Fox Red Labs: AM263P4 R5F SoC hooks. The interrupt calls go to Zephyr's TI VIM driver (the AM263P's VIM is at
 * 0x50F00000, set in the devicetree). Unlike Zephyr's AM64x R5 support, nothing here unlocks K3 control registers:
 * the AM263P has no such partitions at those addresses. */
#include <zephyr/kernel.h>
#include <zephyr/cache.h>
#include "soc.h"

/* Boot progress words for bring-up, read over JTAG at 0x87F00 (top of core 0's B-TCM, not cached):
 * [0] early hook reached, [1] VIM init entered, [2] VIM init returned. CONFIG_FRL_BOOT_MARKS=0 removes them. */
#define FRL_MARK(i, v) (((volatile uint32_t *)0x00087F00u)[(i)] = (v))

unsigned int z_soc_irq_get_active(void) { return z_vim_irq_get_active(); }
void z_soc_irq_eoi(unsigned int irq) { z_vim_irq_eoi(irq); }
void z_soc_irq_init(void) { FRL_MARK(1, 0xA1u); z_vim_irq_init(); FRL_MARK(2, 0xA2u); }
void z_soc_irq_priority_set(unsigned int irq, unsigned int prio, uint32_t flags) { z_vim_irq_priority_set(irq, prio, flags); }
void z_soc_irq_enable(unsigned int irq) { z_vim_irq_enable(irq); }
void z_soc_irq_disable(unsigned int irq) { z_vim_irq_disable(irq); }
int z_soc_irq_is_enabled(unsigned int irq) { return z_vim_irq_is_enabled(irq); }

/* UART0 bring-up with no TI program run first: the values TI's MCU+ SDK 26.01 writes for its LaunchPad hello_world,
 * read back over JTAG after that program ran (2026-10-11). MSS_RCM at 0x53208000: UART0 clock source select 0x333
 * (DPLL_PER_HSDIV0_CLKOUT1) and divider 0x333 give the 48 MHz the devicetree states; the GEL script leaves 0x777 / 0.
 * IOMUX at 0x53100000: UART0 RXD and TXD pads (0x6C, 0x70) = 0x500, mode 0, pull disabled, slow slew. Both blocks
 * are locked behind kick registers; the keys are TI's (soc.h, pinmux.c). */
#if DT_HAS_CHOSEN(zephyr_console)
#define FRL_W(a, v) (*(volatile uint32_t *)(uintptr_t)(a) = (v))
static void frl_uart0_clock_and_pins(void)
{
	FRL_W(0x53208000u + 0x1008u, 0x01234567u);   /* MSS_RCM LOCK0_KICK0 */
	FRL_W(0x53208000u + 0x100Cu, 0x0FEDCBA8u);   /* MSS_RCM LOCK0_KICK1 */
	FRL_W(0x53208000u + 0x174u, 0x333u);         /* LIN0_UART0_CLK_SRC_SEL */
	FRL_W(0x53208000u + 0x274u, 0x333u);         /* LIN0_UART0_CLK_DIV_VAL */
	FRL_W(0x53208000u + 0x38Cu, 0x0u);           /* UART0_CLK_GATE: not gated */
	FRL_W(0x53100000u + 0x298u, 0x83E70B13u);    /* IOMUX IO_CFG_KICK0 */
	FRL_W(0x53100000u + 0x29Cu, 0x95A4F1E0u);    /* IOMUX IO_CFG_KICK1 */
	FRL_W(0x53100000u + 0x6Cu, 0x500u);          /* PIN_UART0_RXD */
	FRL_W(0x53100000u + 0x70u, 0x500u);          /* PIN_UART0_TXD */
}
#endif

void soc_early_init_hook(void)
{
#if DT_HAS_CHOSEN(zephyr_console)
	frl_uart0_clock_and_pins(); /* only the core that owns the console touches UART0 */
#endif
	FRL_MARK(0, 0xA0u);
	FRL_MARK(1, 0); FRL_MARK(2, 0); FRL_MARK(3, 0); FRL_MARK(4, 0);
	sys_cache_data_enable();
	sys_cache_instr_enable();
}

/* No AM263P pin-control driver yet: pins are left as a TI SDK program set them (see the README). */
#include <zephyr/drivers/pinctrl.h>
int pinctrl_configure_pins(const pinctrl_soc_pin_t *pins, uint8_t pin_cnt, uintptr_t reg)
{
	ARG_UNUSED(pins); ARG_UNUSED(pin_cnt); ARG_UNUSED(reg);
	return 0;
}
