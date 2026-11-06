// SPDX-License-Identifier: GPL-2.0
/* zrpmsg: Linux (core 0) side of the shared-memory link to Zephyr on core 1 of the AM62L EVM.
 * Starts the Zephyr image on core 1 with PSCI CPU_ON, owns the two virtio-style vrings in DDR that Linux does not manage (boot with
 * mem=1536M), rings Zephyr's doorbell with SGI 7 and takes Zephyr's doorbell as SPI 868. Control and results through
 * /sys/kernel/zrpmsg/{ctl,result,status}; /dev/zrpmsg carries one message per write and read. Layout: zrpmsg_shm.h. */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/sort.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/arm-smccc.h>
#include <linux/psci.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kernel_read_file.h>
#include <asm/sysreg.h>

typedef u8 uint8_t; typedef u16 uint16_t; typedef u32 uint32_t; typedef u64 uint64_t;
#include "zrpmsg_shm.h"

#define IMG_BASE 0xE0000000UL
#define IMG_MAX  (4UL << 20)
#define IMG_FLUSH (2UL << 20)   /* image, data, bss and stacks of the Zephyr image stay inside the first 2 MB */
#define U16(p) (*(volatile u16 *)(p))
#define U32(p) (*(volatile u32 *)(p))

static u8 *shm;
static struct zr_ctrl *ctrl;
static int virq;
static DECLARE_WAIT_QUEUE_HEAD(zwq);
static DEFINE_MUTEX(zlock);
static u16 tx_avail, tx_last_used, rx_avail, rx_last_used;
static u16 freeq[ZR_NUM];
static int nfree;
static u64 t_isr;
static unsigned long isr_count;
static bool running, lpoll;
static char result[4096];

static inline u64 cnt(void)
{
	u64 v;

	asm volatile("isb\n\tmrs %0, cntvct_el0" : "=r"(v) : : "memory");
	return v;
}

static inline u64 ticks_ns(u64 t)
{
	return t * 1000000000ULL / (ctrl ? 200000000ULL : 200000000ULL);   /* cntfrq is 200 MHz on this chip (arch_timer line in dmesg) */
}

#define V0 (shm + ZR_OFF_VR0)
#define V1 (shm + ZR_OFF_VR1)
#define AV0_IDX (V0 + ZR_VR_AVAIL + 2)
#define AV0_RING(i) (V0 + ZR_VR_AVAIL + 4 + 2 * (i))
#define US0_FLAGS (V0 + ZR_VR_USED)
#define US0_IDX (V0 + ZR_VR_USED + 2)
#define US0_RING(i) (V0 + ZR_VR_USED + 4 + 8 * (i))
#define AV1_FLAGS (V1 + ZR_VR_AVAIL)
#define AV1_IDX (V1 + ZR_VR_AVAIL + 2)
#define AV1_RING(i) (V1 + ZR_VR_AVAIL + 4 + 2 * (i))
#define US1_IDX (V1 + ZR_VR_USED + 2)
#define US1_RING(i) (V1 + ZR_VR_USED + 4 + 8 * (i))
#define BUF0(id) (shm + ZR_OFF_BUF0 + (id) * ZR_BUFSZ)
#define BUF1(id) (shm + ZR_OFF_BUF1 + (id) * ZR_BUFSZ)

static irqreturn_t zr_isr(int irq, void *d)
{
	t_isr = cnt();
	isr_count++;
	wake_up(&zwq);
	return IRQ_HANDLED;
}

static void kick(void)
{
	/* ICC_SGI1R_EL1: INTID in bits 27:24, target list bit 1 = Aff0 1 (core 1) */
	u64 v = ((u64)ZR_SGI << 24) | (1ULL << 1);

	wmb();
	write_sysreg_s(v, SYS_ICC_SGI1R_EL1);
	isb();
}

/* Clean and invalidate [p, p+len) by virtual address down to the point of coherency. The previous run on core 1 leaves its lines in the shared L2;
 * without this the next image is read through stale text and data. */
static void dcache_flush_range(void *p, size_t len)
{
	u64 ctr, line, a, end = (u64)p + len;

	asm volatile("mrs %0, ctr_el0" : "=r"(ctr));
	line = 4ULL << ((ctr >> 16) & 0xf);
	for (a = (u64)p & ~(line - 1); a < end; a += line)
		asm volatile("dc civac, %0" : : "r"(a) : "memory");
	asm volatile("dsb sy" ::: "memory");
}

static void rings_init(void)
{
	int i;

	dcache_flush_range(shm, ZR_SIZE - 0x1000);
	memset(shm, 0, ZR_SIZE - 0x1000);   /* the top page is the console ring: leave it */
	for (i = 0; i < ZR_NUM; i++) {
		u8 *d0 = V0 + ZR_VR_DESC + 16 * i, *d1 = V1 + ZR_VR_DESC + 16 * i;

		*(u64 *)d0 = ZR_BASE + ZR_OFF_BUF0 + (u64)i * ZR_BUFSZ; *(u32 *)(d0 + 8) = ZR_BUFSZ;
		*(u64 *)d1 = ZR_BASE + ZR_OFF_BUF1 + (u64)i * ZR_BUFSZ; *(u32 *)(d1 + 8) = ZR_BUFSZ;
		U16(AV1_RING(i)) = i;
		freeq[i] = i;
	}
	nfree = ZR_NUM; tx_avail = 0; tx_last_used = 0; rx_avail = ZR_NUM; rx_last_used = 0;
	U16(AV1_IDX) = ZR_NUM;
	U16(AV1_FLAGS) = lpoll ? ZR_F_NO_INTERRUPT : 0;
	wmb();
	ctrl->magic = ZR_MAGIC;
	wmb();
}

static int tx_get(void)
{
	if (!nfree) {
		u16 ui = U16(US0_IDX);

		rmb();
		while (tx_last_used != ui) {
			freeq[nfree++] = U32(US0_RING(tx_last_used % ZR_NUM));
			tx_last_used++;
		}
	}
	return nfree ? freeq[--nfree] : -1;
}

/* Send one message of payload length len (>= ZR_MSG_MIN). Returns the CNTVCT stamped into it, or 0 when no buffer is free. */
static u64 zr_send(u32 op, u32 seq, u32 len, u32 a0, u32 a1, bool dokick)
{
	int id = tx_get();
	struct zr_hdr *h;
	struct zr_msg *m;
	u64 t;

	if (id < 0)
		return 0;
	h = (struct zr_hdr *)BUF0(id); m = (struct zr_msg *)(BUF0(id) + 16);
	h->src = 1; h->dst = 2; h->reserved = 0; h->len = len; h->flags = 0;
	m->op = op; m->seq = seq; m->a0 = a0; m->a1 = a1;
	if (len > ZR_MSG_MIN) {   /* pattern in the first and last payload bytes for the receiver to check */
		((u8 *)m)[ZR_MSG_MIN] = (u8)seq;
		((u8 *)m)[len - 1] = (u8)seq ^ 0x5A;
	}
	U16(AV0_RING(tx_avail % ZR_NUM)) = id;
	wmb();
	t = cnt();
	m->t_tx = t;
	wmb();
	tx_avail++;
	U16(AV0_IDX) = tx_avail;
	mb();
	if (dokick && !(U16(US0_FLAGS) & ZR_F_NO_NOTIFY))
		kick();
	return t;
}

static bool rx_ready(void)
{
	return U16(US1_IDX) != rx_last_used;
}

/* take the next received message; the caller reads it and then calls rx_post(id) */
static int rx_take(u32 *len)
{
	int id;

	if (!rx_ready())
		return -1;
	rmb();
	id = U32(US1_RING(rx_last_used % ZR_NUM));
	*len = U32(US1_RING(rx_last_used % ZR_NUM) + 4);
	rx_last_used++;
	return id;
}

static void rx_post(int id)
{
	U16(AV1_RING(rx_avail % ZR_NUM)) = id;
	wmb();
	rx_avail++;
	U16(AV1_IDX) = rx_avail;
	wmb();
}

/* wait until a message is there: spin when polling, sleep on the SPI otherwise. Returns false on timeout (200 ms). */
static bool rx_wait(void)
{
	if (lpoll) {
		u64 t0 = cnt();

		while (!rx_ready()) {
			cpu_relax();
			if (cnt() - t0 > 40000000ULL)
				return false;
		}
		return true;
	}
	return wait_event_timeout(zwq, rx_ready(), HZ / 5) != 0;
}

static void rx_drain(void)   /* drop stale replies so that each command starts from an empty receive side */
{
	u32 len;
	int id;

	while ((id = rx_take(&len)) >= 0)
		rx_post(id);
}

static void set_lmode(bool poll)
{
	lpoll = poll;
	U16(AV1_FLAGS) = poll ? ZR_F_NO_INTERRUPT : 0;
	wmb();
}

static int cmp64(const void *a, const void *b)
{
	u64 x = *(const u64 *)a, y = *(const u64 *)b;

	return x < y ? -1 : x > y;
}

static void stat_line(char *out, size_t sz, const char *name, u64 *v, int n)
{
	sort(v, n, sizeof(u64), cmp64, NULL);
	scnprintf(out + strlen(out), sz - strlen(out), "%-10s median %6llu  min %6llu  p90 %6llu  p99 %6llu  max %7llu ns\n", name,
		  ticks_ns(v[n / 2]), ticks_ns(v[0]), ticks_ns(v[n * 9 / 10]), ticks_ns(v[n * 99 / 100]), ticks_ns(v[n - 1]));
}

/* one request and one reply, returns the reply's op or -1; fills *r with a copy of the reply message */
static int rpc(u32 op, u32 a0, u32 a1, struct zr_msg *r)
{
	u32 len;
	int id;

	u64 t0 = cnt();

	while (!zr_send(op, 0, ZR_MSG_MIN, a0, a1, true)) {   /* all 128 buffers may still be queued behind a long burst: wait for one */
		cpu_relax();
		if (cnt() - t0 > 40000000ULL)
			return -1;
	}
	if (!rx_wait())
		return -1;
	id = rx_take(&len);
	if (id < 0)
		return -1;
	memcpy(r, BUF1(id) + 16, sizeof(*r));
	rx_post(id);
	return r->op;
}

static void bench_rtt(u32 size, u32 n)
{
	u64 *rtt, *l2z, *zp, *z2l, *tow;
	u32 i, warm = 64, bad = 0;
	u64 *buf = kvmalloc_array(5 * n, sizeof(u64), GFP_KERNEL);

	if (!buf) {
		scnprintf(result, sizeof(result), "no memory\n");
		return;
	}
	rtt = buf; l2z = buf + n; zp = buf + 2 * n; z2l = buf + 3 * n; tow = buf + 4 * n;
	if (size < ZR_MSG_MIN) size = ZR_MSG_MIN;
	if (size > ZR_PAYLOAD_MAX) size = ZR_PAYLOAD_MAX;
	for (i = 0; i < n + warm; i++) {
		u64 t0, th;
		u32 len;
		int id;
		struct zr_msg *m;

		if (lpoll)
			preempt_disable();
		t0 = zr_send(ZOP_ECHO, i, size, 0, 0, true);
		if (!t0 || !rx_wait()) {
			if (lpoll)
				preempt_enable();
			bad++;
			break;
		}
		th = lpoll ? cnt() : t_isr;
		id = rx_take(&len);
		if (lpoll)
			preempt_enable();
		m = (struct zr_msg *)(BUF1(id) + 16);
		if (m->seq != i || len != size + 16) bad++;
		if (i >= warm) {
			u32 k = i - warm;

			rtt[k] = th - t0; l2z[k] = m->t_rx_z - t0; zp[k] = m->t_tx_z - m->t_rx_z; z2l[k] = th - m->t_tx_z;
			tow[k] = cnt() - t0;   /* until the thread runs again (equals rtt plus the wake cost when sleeping) */
		}
		rx_post(id);
	}
	result[0] = 0;
	scnprintf(result, sizeof(result), "rtt size=%u n=%u lmode=%s zmode=%u bad=%u irqs=%lu\n", size, n, lpoll ? "poll" : "irq", ctrl->zmode, bad, isr_count);
	if (i == n + warm) {
		stat_line(result, sizeof(result), "rtt", rtt, n);
		stat_line(result, sizeof(result), "to-thread", tow, n);
		stat_line(result, sizeof(result), "linux>zeph", l2z, n);
		stat_line(result, sizeof(result), "zeph-proc", zp, n);
		stat_line(result, sizeof(result), "zeph>linux", z2l, n);
	}
	kvfree(buf);
}

static void bench_lz(u32 size, u32 n)
{
	struct zr_msg r;
	u64 t0, t1;
	u32 i;
	int op = -1;

	if (size < ZR_MSG_MIN) size = ZR_MSG_MIN;
	if (size > ZR_PAYLOAD_MAX) size = ZR_PAYLOAD_MAX;
	rpc(ZOP_STATS, 1, 0, &r);   /* zero Zephyr's sink counters */
	t0 = cnt();
	for (i = 0; i < n; i++) {
		u64 g = 0;

		while (!(g = zr_send(ZOP_SINK, i, size, 0, 0, true))) {
			cpu_relax();
			if (cnt() - t0 > 400000000ULL) goto out;
		}
	}
	op = rpc(ZOP_STATS, 0, 0, &r);
	t1 = cnt();
out:
	scnprintf(result, sizeof(result), "tput linux>zephyr size=%u n=%u lmode=%s zmode=%u\n", size, n, lpoll ? "poll" : "irq", ctrl->zmode);
	if (i == n && op == ZOP_STATS) {
		u64 ns = ticks_ns(t1 - t0);

		scnprintf(result + strlen(result), sizeof(result) - strlen(result),
			  "msgs %llu bytes %llu elapsed %llu ns  %llu msgs/s  %llu kB/s (payload bytes, 1 kB = 1000)\n",
			  r.b0, r.b1, ns, r.b0 * 1000000000ULL / ns, r.b1 * 1000000ULL / ns);
		if (r.b0 != n) strcat(result, "COUNT MISMATCH\n");
	} else {
		scnprintf(result + strlen(result), sizeof(result) - strlen(result),
			  "failed or timed out: sent %u of %u, op %d, tx_avail %u tx_last_used %u used0 %u nfree %d z_rx_msgs %u\n", i, n, op, tx_avail,
			  tx_last_used, U16(US0_IDX), nfree, ctrl->rx_msgs);
	}
}

static void bench_zl(u32 size, u32 n)
{
	u64 t0, t1 = 0;
	u32 got = 0, bad = 0, len;
	bool done = false;

	if (size < ZR_MSG_MIN) size = ZR_MSG_MIN;
	if (size > ZR_PAYLOAD_MAX) size = ZR_PAYLOAD_MAX;
	t0 = zr_send(ZOP_SRC, 0, ZR_MSG_MIN, n, size, true);
	while (!done) {
		int id;

		if (!rx_ready() && !rx_wait())
			break;
		while ((id = rx_take(&len)) >= 0) {
			struct zr_msg *m = (struct zr_msg *)(BUF1(id) + 16);

			if (m->op == ZOP_DATA) {
				u8 *p = (u8 *)m;

				if (m->seq != got || len != size + 16 || (size > ZR_MSG_MIN && (p[ZR_MSG_MIN] != (u8)got || p[size - 1] != (u8)(got ^ 0x5A))))
					bad++;
				got++;
				if (got == n)
					t1 = cnt();
			} else if (m->op == ZOP_DONE) {
				done = true;
			}
			rx_post(id);
		}
	}
	scnprintf(result, sizeof(result), "tput zephyr>linux size=%u n=%u lmode=%s zmode=%u\n", size, n, lpoll ? "poll" : "irq", ctrl->zmode);
	if (got == n && t1) {
		u64 ns = ticks_ns(t1 - t0);

		scnprintf(result + strlen(result), sizeof(result) - strlen(result),
			  "msgs %u bad %u elapsed %llu ns  %llu msgs/s  %llu kB/s (payload bytes, 1 kB = 1000)\n", got, bad, ns,
			  (u64)got * 1000000000ULL / ns, (u64)got * size * 1000000ULL / ns);
	} else {
		scnprintf(result + strlen(result), sizeof(result) - strlen(result), "failed: got %u of %u, bad %u\n", got, n, bad);
	}
}

static int do_load(const char *path, unsigned long entry)
{
	void *img = NULL, *dst;
	size_t sz;
	ssize_t rc;
	struct arm_smccc_res res;
	int i;

	if (running) {
		scnprintf(result, sizeof(result), "already running; send stop first\n");
		return -EBUSY;
	}
	rc = kernel_read_file_from_path(path, 0, &img, IMG_MAX, &sz, READING_FIRMWARE);
	if (rc < 0) {
		scnprintf(result, sizeof(result), "read %s failed: %zd\n", path, rc);
		return rc;
	}
	dst = memremap(IMG_BASE, IMG_FLUSH, MEMREMAP_WC);
	if (!dst) {
		vfree(img);
		return -ENOMEM;
	}
	dcache_flush_range(dst, IMG_FLUSH);
	memcpy(dst, img, sz);
	wmb();
	if (memcmp(dst, img, sz)) {
		scnprintf(result, sizeof(result), "readback of the image differs\n");
		memunmap(dst); vfree(img);
		return -EIO;
	}
	memunmap(dst); vfree(img);
	set_lmode(false);
	rings_init();
	asm volatile("dsb sy" ::: "memory");
	arm_smccc_smc(0xC4000003UL, 1, entry, 0, 0, 0, 0, 0, &res);
	if (res.a0) {
		scnprintf(result, sizeof(result), "PSCI CPU_ON returned %ld\n", (long)res.a0);
		return -EIO;
	}
	for (i = 0; i < 400 && ctrl->status != ZR_ST_READY; i++)
		msleep(5);
	running = ctrl->status == ZR_ST_READY;
	scnprintf(result, sizeof(result), "loaded %zu bytes, CPU_ON ok, %s after %d ms (EL%u, cntfrq %llu)\n", sz, running ? "READY" : "NOT ready",
		  i * 5, ctrl->el, (unsigned long long)ctrl->cntfrq);
	return running ? 0 : -ETIMEDOUT;
}

static ssize_t ctl_store(struct kobject *k, struct kobj_attribute *a, const char *buf, size_t cnt_)
{
	char cmd[16], path[128];
	unsigned long x = 0, y = 0;
	struct zr_msg r;
	int rc = 0;

	mutex_lock(&zlock);
	if (running)
		rx_drain();
	if (sscanf(buf, "load %127s %lx", path, &x) == 2) {
		rc = do_load(path, x);
	} else if (!running) {
		scnprintf(result, sizeof(result), "not running: load first\n");
		rc = -ENODEV;
	} else if (sscanf(buf, "mode %lu %lu", &x, &y) == 2) {
		set_lmode(y != 0);   /* Linux flag first so that the reply is not lost */
		rc = rpc(ZOP_MODE, x, y, &r) == ZOP_MODE ? 0 : -EIO;
		scnprintf(result, sizeof(result), "mode zephyr=%s linux=%s %s\n", x ? "poll" : "irq", y ? "poll" : "irq", rc ? "FAILED" : "ok");
	} else if (sscanf(buf, "%15s %lu %lu", cmd, &x, &y) == 3) {
		if (!strcmp(cmd, "rtt")) bench_rtt(x, y);
		else if (!strcmp(cmd, "lz")) bench_lz(x, y);
		else if (!strcmp(cmd, "zl")) bench_zl(x, y);
		else rc = -EINVAL;
	} else if (sysfs_streq(buf, "stop")) {
		rc = rpc(ZOP_STOP, 0, 0, &r) == ZOP_STOP ? 0 : -EIO;
		msleep(50);
		running = false;
		ctrl->status = 0;
		scnprintf(result, sizeof(result), "stop sent %s\n", rc ? "FAILED" : "ok");
	} else if (sysfs_streq(buf, "ping")) {
		u64 t0 = cnt();

		rc = rpc(ZOP_ECHO, 0, 0, &r);
		scnprintf(result, sizeof(result), "ping reply op %d after %llu ns\n", rc, ticks_ns(cnt() - t0));
	} else {
		rc = -EINVAL;
	}
	mutex_unlock(&zlock);
	return rc < 0 ? rc : cnt_;
}

static ssize_t result_show(struct kobject *k, struct kobj_attribute *a, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s", result);
}

static ssize_t status_show(struct kobject *k, struct kobj_attribute *a, char *buf)
{
	u64 z = ctrl->ztime, now = cnt();

	return scnprintf(buf, PAGE_SIZE, "running %d status 0x%x zmode %u el %u cntfrq %llu hb %u rx_msgs %u z_isr %u spi_irqs %lu ztime_minus_linux %lld ticks lpoll %d\n",
			 running, ctrl->status, ctrl->zmode, ctrl->el, (unsigned long long)ctrl->cntfrq, ctrl->hb, ctrl->rx_msgs, ctrl->isr_count, isr_count,
			 (long long)(z - now), lpoll);
}

static struct kobj_attribute a_ctl = __ATTR_WO(ctl);
static struct kobj_attribute a_result = __ATTR_RO(result);
static struct kobj_attribute a_status = __ATTR_RO(status);
static struct attribute *attrs[] = { &a_ctl.attr, &a_result.attr, &a_status.attr, NULL };
static const struct attribute_group grp = { .attrs = attrs };
static struct kobject *kobj;

/* /dev/zrpmsg: write sends one ECHO message (padded to the 56-byte minimum), read returns the next message's payload */
static ssize_t dev_write(struct file *f, const char __user *ub, size_t n, loff_t *o)
{
	u8 tmp[ZR_PAYLOAD_MAX];
	u32 len = max_t(size_t, n, ZR_MSG_MIN);
	int id;

	if (!running || n > ZR_PAYLOAD_MAX)
		return -EINVAL;
	mutex_lock(&zlock);
	if (copy_from_user(tmp, ub, n)) {
		mutex_unlock(&zlock);
		return -EFAULT;
	}
	if (!zr_send(ZOP_ECHO, 0, len, 0, 0, true)) {
		mutex_unlock(&zlock);
		return -EAGAIN;
	}
	id = -1;
	(void)id; (void)tmp;
	mutex_unlock(&zlock);
	return n;
}

static ssize_t dev_read(struct file *f, char __user *ub, size_t n, loff_t *o)
{
	u32 len;
	int id;
	ssize_t rc;

	if (!running)
		return -EINVAL;
	if (wait_event_interruptible_timeout(zwq, rx_ready(), HZ) <= 0 && !rx_ready())
		return -EAGAIN;
	mutex_lock(&zlock);
	id = rx_take(&len);
	if (id < 0) {
		mutex_unlock(&zlock);
		return -EAGAIN;
	}
	rc = min_t(size_t, n, len - 16);
	if (copy_to_user(ub, BUF1(id) + 16, rc))
		rc = -EFAULT;
	rx_post(id);
	mutex_unlock(&zlock);
	return rc;
}

static const struct file_operations fops = { .owner = THIS_MODULE, .write = dev_write, .read = dev_read };
static struct miscdevice mdev = { .minor = MISC_DYNAMIC_MINOR, .name = "zrpmsg", .fops = &fops };

static int __init zr_init(void)
{
	struct device_node *gic = of_find_compatible_node(NULL, NULL, "arm,gic-v3");
	struct irq_fwspec fw = { .param_count = 3, .param = { 0, ZR_SPI, IRQ_TYPE_EDGE_RISING } };
	int rc;

	if (!gic)
		return -ENODEV;
	fw.fwnode = of_fwnode_handle(gic);
	shm = memremap(ZR_BASE, ZR_SIZE, MEMREMAP_WC);
	if (!shm)
		return -ENOMEM;
	ctrl = (struct zr_ctrl *)shm;
	virq = irq_create_fwspec_mapping(&fw);
	if (virq <= 0) {
		memunmap(shm);
		return -EINVAL;
	}
	rc = request_irq(virq, zr_isr, 0, "zrpmsg", &zwq);
	if (rc) {
		memunmap(shm);
		return rc;
	}
	kobj = kobject_create_and_add("zrpmsg", kernel_kobj);
	if (!kobj || sysfs_create_group(kobj, &grp))
		return -ENOMEM;
	misc_register(&mdev);
	pr_info("zrpmsg: window %lx, SPI %d is Linux irq %d\n", ZR_BASE, ZR_SPI, virq);
	return 0;
}

static void __exit zr_exit(void)
{
	misc_deregister(&mdev);
	sysfs_remove_group(kobj, &grp);
	kobject_put(kobj);
	free_irq(virq, &zwq);
	memunmap(shm);
}
module_init(zr_init);
module_exit(zr_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Linux to Zephyr shared-memory link, AM62L EVM");
