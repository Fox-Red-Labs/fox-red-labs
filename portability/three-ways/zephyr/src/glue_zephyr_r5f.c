/* Three ways, Zephyr glue for the BeagleY-AI MCU-domain R5F (loaded by Linux remoteproc).
 * Output is Zephyr's RAM console (printk) in a fixed buffer that the resource table declares as a
 * remoteproc trace buffer, so Linux shows it in debugfs remoteproc2/trace0. The chip has no ADC
 * on this board, so adc_read() returns 65535 as the Linux glue does; the LED is not driven. */
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/cache.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/mbox.h>
#include "app.h"

#define TRACE_ADDR DT_REG_ADDR(DT_CHOSEN(zephyr_ram_console))
#define TRACE_SIZE CONFIG_RAM_CONSOLE_BUFFER_SIZE

/* remoteproc resource table (include/linux/remoteproc.h): one RSC_TRACE entry */
struct rsc_trace {
    uint32_t type, da, len, reserved;
    char name[32];
};
struct rsc_table {
    uint32_t ver, num, reserved[2];
    uint32_t offset[1];
    struct rsc_trace trace;
};
__attribute__((used, section(".resource_table"))) struct rsc_table resource_table = {
    .ver = 1, .num = 1, .offset = {offsetof(struct rsc_table, trace)},
    .trace = {.type = 2 /* RSC_TRACE */, .da = TRACE_ADDR, .len = TRACE_SIZE, .name = "trace:r5f0"},
};

/* Linux remoteproc (k3_r5) stops a core by sending RP_MBOX_SHUTDOWN on mailbox FIFO 1 and waits for
 * RP_MBOX_SHUTDOWN_ACK on FIFO 0, then for the core to be in WFI (2 ms) before it halts it.
 * FIFO 1 keeps messages across a Linux reboot (an old echo request was there at every start), so it is
 * drained first. */
#define RP_MBOX_SHUTDOWN     0xFFFFFF14u
#define RP_MBOX_SHUTDOWN_ACK 0xFFFFFF15u
static const struct mbox_dt_spec mbox_tx = MBOX_DT_SPEC_GET(DT_CHOSEN(zephyr_ipc), tx);
static const struct mbox_dt_spec mbox_rx = MBOX_DT_SPEC_GET(DT_CHOSEN(zephyr_ipc), rx);

static void mbox_cb(const struct device *dev, mbox_channel_id_t ch, void *user, struct mbox_msg *msg)
{
    uint32_t v = msg && msg->data ? *(const uint32_t *) msg->data : 0;
    if (v == RP_MBOX_SHUTDOWN) {
        struct mbox_msg ack = {.data = &(uint32_t){RP_MBOX_SHUTDOWN_ACK}, .size = 4};
        mbox_send_dt(&mbox_tx, &ack);
        for (;;) {
            __asm__ volatile("wfi");
        }
    }
}

/* The MPU background map treats the mailbox page (and other peripherals above the first MPU region) as
 * cacheable normal memory, so register reads return stale cache lines and writes sit in the cache. Make
 * the mailbox page shareable device memory (MPU region 11, 4 KB). */
static void mailbox_page_device(void)
{
    sys_cache_data_invd_range((void *) 0x29010000u, 0x1000);
    __asm__ volatile("mcr p15, 0, %0, c6, c2, 0" ::"r"(11u));          /* RGNR */
    __asm__ volatile("mcr p15, 0, %0, c6, c1, 0" ::"r"(0x29010000u));  /* DRBAR */
    __asm__ volatile("mcr p15, 0, %0, c6, c1, 4" ::"r"(0x1301u));      /* DRACR: device, full access, XN */
    __asm__ volatile("mcr p15, 0, %0, c6, c1, 2" ::"r"(0x17u));        /* DRSR: 4 KB, enable */
    __asm__ volatile("dsb\n\tisb" ::: "memory");
}

/* Tick timer fix. The MCU timer 0 (04800000) runs from Linux-started core, but Zephyr's ti_dmtimer driver
 * writes TCLR several times in a row; the timer's registers are write-posted (functional clock 25 MHz),
 * so the write that sets the compare-enable bit (CE) is dropped (TCLR reads 3, not 0x43) and the
 * match interrupt never fires. Set CE again, waiting for TWPS to clear, and arm one near match. */
#define TMR(o) (*(volatile uint32_t *) (0x04800000u + (o)))
static void timer_fix(void)
{
    while (TMR(0x48)) {
    }
    TMR(0x38) |= 0x40;
    while (TMR(0x48)) {
    }
    TMR(0x4c) = TMR(0x3c) + 2500u;
    while (TMR(0x48)) {
    }
}

void board_init(void)
{
    sys_cache_data_flush_range((void *) &resource_table, sizeof(resource_table));
    timer_fix();
    mailbox_page_device();
    for (int i = 0; i < 8 && *(volatile uint32_t *) 0x290100c4u; i++) {
        (void) *(volatile uint32_t *) 0x29010044u;   /* pop FIFO 1 */
    }
    mbox_register_callback_dt(&mbox_rx, mbox_cb, NULL);
    mbox_set_enabled_dt(&mbox_rx, true);
}

uint16_t adc_read(void)
{
    return 65535;
}

void uart_write(const char *s, uint32_t len)
{
    printk("%.*s", (int) len, s);
    sys_cache_data_flush_range((void *) TRACE_ADDR, TRACE_SIZE);
}

void led_toggle(void)
{

}

void sleep_ms(uint32_t ms)
{
    k_msleep(ms);
}

int main(void)
{
    board_init();
    app_run();
    return 0;
}
