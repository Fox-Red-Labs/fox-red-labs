/* Link F: the Zephyr side of the Linux (core 0) to Zephyr (core 1) message link on the AM62L EVM.
 * Device side of two virtio split vrings in shared DDR (layout: am62l-evm/rpmsg/zrpmsg_shm.h). Linux sends with SGI 7; this core answers
 * through SPI 868 set pending in GICD_ISPENDR. Either direction can be polled instead. Console goes to the ring at 0xEFFFF000. */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/irq.h>
#include <stdint.h>
#include <string.h>
#include "zrpmsg_shm.h"

#define SHM ((uintptr_t)ZR_BASE)
#define N ZR_NUM
#define R16(a) (*(volatile uint16_t *)(a))
#define R32(a) (*(volatile uint32_t *)(a))
#define DMB() __asm__ volatile("dmb sy" ::: "memory")
#define DSB() __asm__ volatile("dsb sy" ::: "memory")

static struct zr_ctrl *const ctrl = (struct zr_ctrl *)(SHM + ZR_OFF_CTRL);
#define V0 (SHM + ZR_OFF_VR0)
#define V1 (SHM + ZR_OFF_VR1)
#define AV0_FLAGS (V0 + ZR_VR_AVAIL)
#define AV0_IDX   (V0 + ZR_VR_AVAIL + 2)
#define AV0_RING(i) (V0 + ZR_VR_AVAIL + 4 + 2 * (i))
#define US0_FLAGS (V0 + ZR_VR_USED)
#define US0_IDX   (V0 + ZR_VR_USED + 2)
#define US0_RING(i) (V0 + ZR_VR_USED + 4 + 8 * (i))
#define AV1_FLAGS (V1 + ZR_VR_AVAIL)
#define AV1_IDX   (V1 + ZR_VR_AVAIL + 2)
#define AV1_RING(i) (V1 + ZR_VR_AVAIL + 4 + 2 * (i))
#define US1_IDX   (V1 + ZR_VR_USED + 2)
#define US1_RING(i) (V1 + ZR_VR_USED + 4 + 8 * (i))
#define BUF0(id) (SHM + ZR_OFF_BUF0 + (uintptr_t)(id) * ZR_BUFSZ)
#define BUF1(id) (SHM + ZR_OFF_BUF1 + (uintptr_t)(id) * ZR_BUFSZ)

static uint16_t last_avail0, used0, last_avail1, used1;
static volatile bool stop_req;
static uint64_t sink_msgs, sink_bytes, sink_first, sink_last;

static inline uint64_t cnt(void)
{
	uint64_t v;

	__asm__ volatile("isb\n\tmrs %0, cntvct_el0" : "=r"(v));
	return v;
}

static void notify_linux(void)
{
	if (R16(AV1_FLAGS) & ZR_F_NO_INTERRUPT) {
		return;
	}
	DSB();
	sys_write32(1U << ((ZR_SPI + 32) % 32), ZR_GICD_BASE + 0x200 + 4 * ((ZR_SPI + 32) / 32));
}

static int rx_buf_get(void)   /* an empty buffer Linux posted on vring 1, or -1 */
{
	if (last_avail1 == R16(AV1_IDX)) {
		return -1;
	}
	DMB();
	return R16(AV1_RING(last_avail1++ % N));
}

static void rx_buf_put(int id, uint32_t len)
{
	R32(US1_RING(used1 % N)) = (uint32_t)id;
	R32(US1_RING(used1 % N) + 4) = len;
	DMB();
	used1++;
	R16(US1_IDX) = used1;
	DMB();
}

static int send_wait_buf(void)
{
	int id;
	uint64_t t0 = cnt();

	while ((id = rx_buf_get()) < 0) {
		if (cnt() - t0 > 2 * 200000000ULL) {
			return -1;
		}
	}
	return id;
}

static struct zr_msg *out_msg(int id, uint32_t len)
{
	struct zr_hdr *h = (struct zr_hdr *)BUF1(id);

	h->src = 2; h->dst = 1; h->reserved = 0; h->len = (uint16_t)len; h->flags = 0;
	return (struct zr_msg *)(BUF1(id) + 16);
}

static void cpu_off(void)
{
	register uint64_t x0 __asm__("x0") = 0x84000002ULL;   /* PSCI CPU_OFF */

	__asm__ volatile("smc #0" : "+r"(x0) : : "memory");
	for (;;) {
		__asm__ volatile("wfi");
	}
}

static void reply_copy(const struct zr_hdr *h, const struct zr_msg *m, uint64_t t_rx)
{
	int id = send_wait_buf();

	if (id < 0) {
		return;
	}
	uint32_t len = h->len;
	struct zr_hdr *oh = (struct zr_hdr *)BUF1(id);
	volatile uint64_t *dst = (volatile uint64_t *)(BUF1(id) + 16), *src = (volatile uint64_t *)m;

	for (uint32_t i = 0; i < (len + 7) / 8; i++) {   /* 64-bit words: each access to non-cacheable DDR costs a full memory round trip */
		dst[i] = src[i];
	}
	oh->src = 2; oh->dst = 1; oh->reserved = 0; oh->len = (uint16_t)len; oh->flags = 0;
	struct zr_msg *o = (struct zr_msg *)(BUF1(id) + 16);

	o->t_rx_z = t_rx;
	o->t_tx_z = cnt();
	rx_buf_put(id, len + 16);
	notify_linux();
}

static void handle(int id, uint32_t len_used)
{
	struct zr_hdr *h = (struct zr_hdr *)BUF0(id);
	struct zr_msg *m = (struct zr_msg *)(BUF0(id) + 16);
	uint64_t t_rx = cnt();
	uint32_t op = m->op;

	ARG_UNUSED(len_used);
	ctrl->rx_msgs++;
	switch (op) {
	case ZOP_ECHO:
	case ZOP_NOP:
		reply_copy(h, m, t_rx);
		break;
	case ZOP_SINK:
		if (sink_msgs == 0) {
			sink_first = t_rx;
		}
		sink_msgs++;
		sink_bytes += h->len;
		sink_last = t_rx;
		break;
	case ZOP_STATS: {
		int o = send_wait_buf();

		if (o >= 0) {
			struct zr_msg *r = out_msg(o, ZR_MSG_MIN);

			r->op = ZOP_STATS; r->seq = m->seq; r->t_tx = m->t_tx; r->t_rx_z = sink_first; r->t_tx_z = sink_last;
			r->a0 = (uint32_t)sink_msgs; r->a1 = 0; r->b0 = sink_msgs; r->b1 = sink_bytes;
			rx_buf_put(o, 16 + ZR_MSG_MIN);
			notify_linux();
		}
		if (m->a0 == 1) {   /* reset counters */
			sink_msgs = sink_bytes = sink_first = sink_last = 0;
		}
		break;
	}
	case ZOP_SRC: {
		uint32_t n = m->a0, s = m->a1, seq = m->seq;
		uint64_t first = 0, last = 0;

		if (s < ZR_MSG_MIN) {
			s = ZR_MSG_MIN;
		}
		if (s > ZR_PAYLOAD_MAX) {
			s = ZR_PAYLOAD_MAX;
		}
		for (uint32_t i = 0; i < n; i++) {
			int o = send_wait_buf();

			if (o < 0) {
				break;
			}
			struct zr_msg *r = out_msg(o, s);

			r->op = ZOP_DATA; r->seq = i; r->t_tx_z = last = cnt();
			if (i == 0) {
				first = last;
			}
			uint8_t *p = (uint8_t *)r;

			if (s > ZR_MSG_MIN) {   /* first and last payload bytes carry a pattern for Linux to check */
				p[ZR_MSG_MIN] = (uint8_t)i;
				p[s - 1] = (uint8_t)(i ^ 0x5A);
			}
			rx_buf_put(o, 16 + s);
			notify_linux();
		}
		int o = send_wait_buf();

		if (o >= 0) {
			struct zr_msg *r = out_msg(o, ZR_MSG_MIN);

			r->op = ZOP_DONE; r->seq = seq; r->b0 = first; r->b1 = last; r->t_tx_z = cnt();
			rx_buf_put(o, 16 + ZR_MSG_MIN);
			notify_linux();
		}
		break;
	}
	case ZOP_MODE:
		ctrl->zmode = m->a0;
		ctrl->lmode = m->a1;
		DMB();
		reply_copy(h, m, t_rx);
		break;
	case ZOP_STOP:
		reply_copy(h, m, t_rx);
		stop_req = true;   /* CPU_OFF runs from the main loop, never inside the SGI handler: a handler cut off by CPU_OFF leaves the SGI active in the redistributor and the next boot never gets it */
		break;
	default:
		break;
	}
}

static void process(void)
{
	for (;;) {
		if (ctrl->zmode == 0) {
			R16(US0_FLAGS) = ZR_F_NO_NOTIFY;
			DMB();
		} else {
			R16(US0_FLAGS) = ZR_F_NO_NOTIFY;
		}
		while (last_avail0 != R16(AV0_IDX)) {
			DMB();
			int id = R16(AV0_RING(last_avail0 % N));

			last_avail0++;
			handle(id, 0);
			R32(US0_RING(used0 % N)) = (uint32_t)id;
			R32(US0_RING(used0 % N) + 4) = 0;
			DMB();
			used0++;
			R16(US0_IDX) = used0;
			DMB();
		}
		if (ctrl->zmode != 0) {
			return;
		}
		R16(US0_FLAGS) = 0;
		DMB();
		if (last_avail0 == R16(AV0_IDX)) {
			return;
		}
	}
}

static void sgi_isr(const void *arg)
{
	ARG_UNUSED(arg);
	ctrl->isr_count++;
	process();
}

int main(void)
{
	uint64_t mpidr, el, frq;

	__asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
	__asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(frq));
	printk("core1-rpmsg: MPIDR=0x%llx EL%llu cntfrq=%llu magic=0x%x\n", (unsigned long long)mpidr,
	       (unsigned long long)(el >> 2), (unsigned long long)frq, ctrl->magic);
	if (ctrl->magic != ZR_MAGIC) {
		printk("core1-rpmsg: no magic, Linux has not set up the window\n");
	}
	{   /* a previous run on this core may have left the doorbell SGI pending or active in the redistributor */
		uintptr_t sgi_base = DT_REG_ADDR_BY_IDX(DT_NODELABEL(gic), 1) + (uintptr_t)(mpidr & 0xff) * 0x20000 + 0x10000;

		sys_write32(1U << ZR_SGI, sgi_base + 0x380);   /* GICR_ICACTIVER0 */
		sys_write32(1U << ZR_SGI, sgi_base + 0x280);   /* GICR_ICPENDR0 */
	}
	IRQ_CONNECT(ZR_SGI, 0, sgi_isr, NULL, 0);
	irq_enable(ZR_SGI);
	ctrl->el = (uint32_t)(el >> 2);
	ctrl->cntfrq = frq;
	ctrl->zmode = 0;
	ctrl->isr_count = 0;
	R16(US0_FLAGS) = 0;
	DMB();
	ctrl->status = ZR_ST_READY;
	DSB();
	printk("core1-rpmsg: ready\n");
	for (;;) {
		if (stop_req) {
			(void)irq_lock();
			printk("core1-rpmsg: stop, PSCI CPU_OFF\n");
			cpu_off();
		}
		ctrl->hb++;
		ctrl->ztime = cnt();
		if (ctrl->zmode != 0) {
			unsigned int key = irq_lock();

			for (int i = 0; i < 1000 && ctrl->zmode != 0; i++) {
				process();
			}
			irq_unlock(key);
		} else {
			k_cpu_idle();
		}
	}
	return 0;
}
