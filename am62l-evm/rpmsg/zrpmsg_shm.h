/* Shared-memory layout of the Linux (core 0) to Zephyr (core 1) link on the AM62L EVM.
 * Included by the Linux module (zrpmsg.c) and the Zephyr app (apps/core1-rpmsg). Fixed-width types come from the includer.
 * Two virtio split vrings in DDR the Linux kernel does not own (boot with mem=1536M): vring 0 carries Linux to Zephyr (Linux is the
 * driver side: it fills buffers and the available ring; Zephyr is the device side: it returns buffers in the used ring), vring 1 carries
 * Zephyr to Linux (Linux posts empty buffers, Zephyr fills them). Each message is an RPMsg header (16 bytes) plus payload in one
 * 512-byte buffer. Doorbells: Linux to Zephyr is SGI ZR_SGI sent with ICC_SGI1R_EL1; Zephyr to Linux is SPI ZR_SPI set pending in
 * GICD_ISPENDR. Either side may poll instead (the ZR_NO_NOTIFY and ZR_NO_INTERRUPT flags follow the virtio meanings). */
#ifndef ZRPMSG_SHM_H
#define ZRPMSG_SHM_H

#define ZR_BASE        0xEFF00000UL   /* 1 MB window, mapped Normal non-cacheable on both sides */
#define ZR_SIZE        0x00100000UL
#define ZR_MAGIC       0x5A525047U    /* 'ZRPG' */
#define ZR_NUM         128            /* buffers per vring */
#define ZR_BUFSZ       512
#define ZR_PAYLOAD_MAX (ZR_BUFSZ - 16)
#define ZR_SGI         7              /* doorbell Linux to Zephyr: SGI 7. Only SGIs 0 to 7 can be enabled from the non-secure world on this chip; SGIs 8 to 15 ignore the enable write (measured). Linux never sends SGI 7 to core 1 itself, because core 1 is not one of its CPUs. */
#define ZR_SPI         868            /* doorbell Zephyr to Linux: SPI 868 = GIC INTID 900 (devicetree numbering) */
#define ZR_GICD_BASE   0x01800000UL

#define ZR_OFF_CTRL    0x0000
#define ZR_OFF_VR0     0x1000         /* Linux to Zephyr */
#define ZR_OFF_VR1     0x2000         /* Zephyr to Linux */
#define ZR_OFF_BUF0    0x10000
#define ZR_OFF_BUF1    0x20000
/* inside a vring page */
#define ZR_VR_DESC     0x000          /* ZR_NUM * 16 bytes */
#define ZR_VR_AVAIL    0x800          /* u16 flags, u16 idx, u16 ring[ZR_NUM], u16 used_event */
#define ZR_VR_USED     0xA00          /* u16 flags, u16 idx, {u32 id, u32 len} ring[ZR_NUM], u16 avail_event */
#define ZR_F_NO_NOTIFY    1           /* used.flags: device does not want a kick */
#define ZR_F_NO_INTERRUPT 1           /* avail.flags: driver does not want an interrupt */

#define ZR_ST_READY    0x52454459U    /* 'REDY' written by Zephyr when it is serving */

/* control block at ZR_BASE + ZR_OFF_CTRL */
struct zr_ctrl {
	volatile uint32_t magic;       /* 0x00 written by Linux before it starts Zephyr */
	volatile uint32_t status;      /* 0x04 ZR_ST_READY from Zephyr */
	volatile uint32_t zmode;       /* 0x08 how Zephyr waits for work: 0 interrupt (SGI), 1 poll */
	volatile uint32_t lmode;       /* 0x0c informational: how Linux waits, 0 interrupt, 1 poll */
	volatile uint32_t hb;          /* 0x10 heartbeat, Zephyr counts main-loop passes (poll mode) or ticks */
	volatile uint32_t rx_msgs;     /* 0x14 messages Zephyr has taken from vring 0 */
	volatile uint64_t ztime;       /* 0x18 Zephyr's CNTVCT at the last heartbeat */
	volatile uint64_t cntfrq;      /* 0x20 counter frequency Zephyr sees */
	volatile uint32_t el;          /* 0x28 CurrentEL >> 2 */
	volatile uint32_t isr_count;   /* 0x2c SGI handler entries */
};

struct zr_hdr {                        /* RPMsg header */
	uint32_t src, dst, reserved;
	uint16_t len, flags;
};

enum zr_op { ZOP_ECHO = 1, ZOP_SINK, ZOP_STATS, ZOP_SRC, ZOP_MODE, ZOP_STOP, ZOP_NOP, ZOP_DATA, ZOP_DONE };

struct zr_msg {                        /* payload, little endian */
	uint32_t op;
	uint32_t seq;
	uint64_t t_tx;                 /* Linux CNTVCT just before the kick */
	uint64_t t_rx_z;               /* Zephyr CNTVCT when it first saw the message */
	uint64_t t_tx_z;               /* Zephyr CNTVCT just before it sent the reply */
	uint32_t a0, a1;               /* arguments / results */
	uint64_t b0, b1;               /* more results */
};
#define ZR_MSG_MIN sizeof(struct zr_msg)   /* 56 bytes */
#endif
