/*
 * mcu-bench: a first benchmark pass on the LP-MSPM0G3507 under Zephyr.
 * This copy (mspm0l1306/zephyr/mcu-bench-l1306) adds the LP-MSPM0L1306: no math accelerator,
 * one GPIO port, 4 KB of RAM. Changes for it are marked "L1306".
 * Prints one line per result on the console UART, then runs CoreMark, then prints "BENCH DONE".
 * Line format: R,<name>,<cycles per operation x100>,<operations>,<note>
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <cmsis_core.h>
#include "nn_weights.h"

/*
 * Board selection. MSPM0G3507 is the original target. CC2340R5 (CC23X0 series) has no math
 * accelerator, a different toggle register, and a different cycle counter.
 */
#if defined(CONFIG_SOC_SERIES_CC23X0)
#define BENCH_CC23 1
#define BENCH_BOARD "lp_em_cc2340r5"
#define BENCH_LED_NAME "DIO15, the green LED"
#include <inc/hw_types.h>
#include <inc/hw_memmap.h>
#include <inc/hw_gpio.h>
#include <inc/hw_systim.h>
#define SPARE_IRQ 6 /* CPUIRQ6: not used by any node in cc23x0.dtsi (NUM_IRQS is 19) */
/*
 * k_cycle_get_32() on CC23X0 returns SYSTIM TIME1U, a 1 MHz count (cc23x0_systim_timer.c), even
 * though CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC says 48 MHz. Read TIME250N instead: the same counter
 * at 4 MHz, so one tick is 12 CPU cycles at 48 MHz. Header: hw_systim.h SYSTIM_O_TIME250N.
 */
#define BENCH_TIMER_HZ 4000000U
#define BENCH_CPU_HZ DT_PROP(DT_PATH(cpus, cpu_0), clock_frequency)
#define BENCH_CYCLES_PER_TICK (BENCH_CPU_HZ / BENCH_TIMER_HZ)
static inline uint32_t bench_now(void)
{
	return HWREG(SYSTIM_BASE + SYSTIM_O_TIME250N);
}
#elif defined(CONFIG_SOC_MSPM0L1306)
/* L1306: no math accelerator; led0 is LED1 on PA0 (open drain, active low). */
#define BENCH_MSPM0 1
#define BENCH_BOARD "lp_mspm0l1306"
#define BENCH_LED_NAME "PA0, LED1"
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/dl_gpio.h>
#define SPARE_IRQ 25 /* I2C1, unused here. The NVIC implements only lines 0, 1, 2, 4, 9, 13, 15, 16, 18, 20, 24, 25, 31 (read back from ISER); pending line 28 does nothing */
#define BENCH_CYCLES_PER_TICK 1U /* k_cycle_get_32() counts CPU cycles */
static inline uint32_t bench_now(void)
{
	return k_cycle_get_32();
}
#else
#define BENCH_MSPM0 1
#define BENCH_MATHACL 1
#define BENCH_BOARD "lp_mspm0g3507"
#define BENCH_LED_NAME "PB22, the blue LED"
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/dl_gpio.h>
#include <ti/driverlib/dl_mathacl.h>
#define SPARE_IRQ 28 /* an interrupt line nothing on this board uses */
#define BENCH_CYCLES_PER_TICK 1U /* k_cycle_get_32() counts CPU cycles */
static inline uint32_t bench_now(void)
{
	return k_cycle_get_32();
}
#endif

#define RUNS 5

/*
 * L1306: 4 KB of RAM. The copy buffers and the neural-net buffers live on the main stack only
 * while their test runs, so the same bytes serve CoreMark afterwards. The copy is 512 bytes
 * instead of 1024: two 1024-byte buffers on the stack would need more main stack than CoreMark.
 */
#if CONFIG_SRAM_SIZE <= 8
#define BENCH_SMALL_RAM 1
#define BENCH_PEER_STACK 256
#define BENCH_COPY_BYTES 512
#else
#define BENCH_PEER_STACK 512
#define BENCH_COPY_BYTES 1024
#endif

#ifndef BENCH_COREMARK_ONLY /* everything from here to coremark_main is for the other tests */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static volatile uint32_t sink;

static void report(const char *name, uint32_t ticks, uint32_t ops, const char *note)
{
	/* timer ticks to CPU cycles: x1 on MSPM0, x12 on CC2340R5 */
	uint32_t x100 = (uint32_t)(((uint64_t)ticks * BENCH_CYCLES_PER_TICK * 100U) / ops);

	printf("R,%s,%u.%02u,%u,%s\n", name, x100 / 100U, x100 % 100U, ops, note);
}

/* Best of RUNS, so a stray timer interrupt does not count. */
#define TIME_LOOP(name, n, note, body)                                                             \
	do {                                                                                       \
		uint32_t best = UINT32_MAX;                                                        \
		for (int r = 0; r < RUNS; r++) {                                                   \
			uint32_t t0 = bench_now();                                            \
			for (uint32_t i = 0; i < (n); i++) {                                       \
				body;                                                              \
			}                                                                          \
			uint32_t dt = bench_now() - t0;                                       \
			if (dt < best) {                                                           \
				best = dt;                                                         \
			}                                                                          \
		}                                                                                  \
		report(name, best, n, note);                                                       \
	} while (0)

/* ---- math accelerator (MSPM0G only) ---- */
#ifdef BENCH_MATHACL
static const DL_MathACL_operationConfig div_cfg = {
	.opType = DL_MATHACL_OP_TYPE_DIV,
	.opSign = DL_MATHACL_OPSIGN_UNSIGNED,
	.iterations = 0,
	.scaleFactor = 0,
	.qType = DL_MATHACL_Q_TYPE_Q0};
static const DL_MathACL_operationConfig sincos_cfg = {
	.opType = DL_MATHACL_OP_TYPE_SINCOS,
	.opSign = DL_MATHACL_OPSIGN_UNSIGNED,
	.iterations = 10,
	.scaleFactor = 0,
	.qType = DL_MATHACL_Q_TYPE_Q31};

static inline uint32_t hw_div(uint32_t n, uint32_t d)
{
	DL_MathACL_startDivOperation(MATHACL, &div_cfg, n, d);
	DL_MathACL_waitForOperation(MATHACL);
	return DL_MathACL_getResultOne(MATHACL);
}

static inline uint32_t hw_sincos(uint32_t angle_q31)
{
	DL_MathACL_startSinCosOperation(MATHACL, &sincos_cfg, angle_q31);
	DL_MathACL_waitForOperation(MATHACL);
	return DL_MathACL_getResultOne(MATHACL) ^ DL_MathACL_getResultTwo(MATHACL);
}

#endif /* BENCH_MATHACL */

/* ---- interrupt latency ---- */
static volatile uint32_t isr_cycle;

static void spare_isr(const void *arg)
{
	ARG_UNUSED(arg);
	isr_cycle = bench_now();
}

/* ---- thread switch ---- */
K_THREAD_STACK_DEFINE(peer_stack, BENCH_PEER_STACK);
static struct k_thread peer_thread;

static void peer_entry(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);
	while (1) {
		k_yield();
	}
}

/* ---- small neural nets, int8 weights, plain C, no accelerator ---- */
static inline int8_t relu_q(int32_t acc)
{
	acc >>= 7;
	if (acc < 0) {
		return 0;
	}
	return acc > 127 ? 127 : (int8_t)acc;
}

#ifdef BENCH_SMALL_RAM
/* L1306: these point into bench_nn()'s stack frame while the nn tests run. Indexing is unchanged. */
static int8_t *nn_in;
static int8_t (*cnn_a1)[61];
static int8_t (*cnn_a2)[29];
#else
static int8_t nn_in[128];
static int8_t cnn_a1[8][61];
static int8_t cnn_a2[16][29];
#endif

static int32_t mlp_infer(const int8_t *x)
{
	int8_t h1[32], h2[16];
	int32_t out = 0;

	for (int o = 0; o < 32; o++) {
		int32_t acc = mlp_b1[o] << 7;

		for (int i = 0; i < 64; i++) {
			acc += mlp_w1[o * 64 + i] * x[i];
		}
		h1[o] = relu_q(acc);
	}
	for (int o = 0; o < 16; o++) {
		int32_t acc = mlp_b2[o] << 7;

		for (int i = 0; i < 32; i++) {
			acc += mlp_w2[o * 32 + i] * h1[i];
		}
		h2[o] = relu_q(acc);
	}
	for (int o = 0; o < 4; o++) {
		int32_t acc = mlp_b3[o] << 7;

		for (int i = 0; i < 16; i++) {
			acc += mlp_w3[o * 16 + i] * h2[i];
		}
		out += acc * (o + 1);
	}
	return out;
}

static int32_t cnn_infer(const int8_t *x)
{
	int32_t out = 0;

	for (int o = 0; o < 8; o++) {
		for (int t = 0; t < 61; t++) {
			int32_t acc = cnn_b1[o] << 7;

			for (int k = 0; k < 8; k++) {
				acc += cnn_w1[o * 8 + k] * x[2 * t + k];
			}
			cnn_a1[o][t] = relu_q(acc);
		}
	}
	for (int o = 0; o < 16; o++) {
		for (int t = 0; t < 29; t++) {
			int32_t acc = cnn_b2[o] << 7;

			for (int c = 0; c < 8; c++) {
				for (int k = 0; k < 4; k++) {
					acc += cnn_w2[(o * 8 + c) * 4 + k] * cnn_a1[c][2 * t + k];
				}
			}
			cnn_a2[o][t] = relu_q(acc);
		}
	}
	for (int o = 0; o < 4; o++) {
		int32_t acc = cnn_b3[o] << 7;
		const int8_t *a = &cnn_a2[0][0];

		for (int i = 0; i < 29 * 16; i++) {
			acc += cnn_w3[o * 464 + i] * a[i];
		}
		out += acc * (o + 1);
	}
	return out;
}

#endif /* BENCH_COREMARK_ONLY */

extern int coremark_main(void);

/* Stack use of the idle thread and the interrupt stack. CONFIG_INIT_STACKS paints both. */
extern struct k_thread z_idle_threads[];
K_KERNEL_STACK_ARRAY_DECLARE(z_interrupt_stacks, CONFIG_MP_MAX_NUM_CPUS, CONFIG_ISR_STACK_SIZE);
int z_stack_space_get(const uint8_t *stack_start, size_t size, size_t *unused_ptr);

static void report_stacks(void)
{
	size_t unused = 0;

	z_stack_space_get(K_KERNEL_STACK_BUFFER(z_interrupt_stacks[0]),
			  K_KERNEL_STACK_SIZEOF(z_interrupt_stacks[0]), &unused);
	printf("I,isr_stack_bytes,%u\n", (unsigned int)CONFIG_ISR_STACK_SIZE);
	printf("I,isr_stack_unused,%u\n", (unsigned int)unused);
	k_thread_stack_space_get(&z_idle_threads[0], &unused);
	printf("I,idle_stack_bytes,%u\n", (unsigned int)CONFIG_IDLE_STACK_SIZE);
	printf("I,idle_stack_unused,%u\n", (unsigned int)unused);
}

#ifndef BENCH_COREMARK_ONLY
/* Memory copy in its own frame. On the L1306 both buffers are on the stack. */
static __noinline void bench_memcpy(void)
{
#ifdef BENCH_SMALL_RAM
	uint8_t src[BENCH_COPY_BYTES], dst[BENCH_COPY_BYTES];
#else
	static uint8_t src[BENCH_COPY_BYTES], dst[BENCH_COPY_BYTES];
#endif

	memset(src, 0x5a, sizeof(src));
	/* the empty asm keeps the compiler from treating the repeated copy as dead */
	TIME_LOOP("memcpy_" STRINGIFY(BENCH_COPY_BYTES) "_bytes", 200,
		  "per " STRINGIFY(BENCH_COPY_BYTES) "-byte copy, RAM to RAM",
		  memcpy(dst, src, sizeof(src)); __asm__ volatile("" ::: "memory");
		  sink += dst[i & (BENCH_COPY_BYTES - 1)]);
}

/* Small neural nets in their own frame. On the L1306 the buffers are on the stack. */
static __noinline void bench_nn(void)
{
#ifdef BENCH_SMALL_RAM
	int8_t in_buf[128], a1_buf[8][61], a2_buf[16][29];

	nn_in = in_buf;
	cnn_a1 = a1_buf;
	cnn_a2 = a2_buf;
#endif
	for (int i = 0; i < 128; i++) {
		nn_in[i] = (int8_t)((i * 37) & 0x7f) - 64;
	}
	{
		int32_t chk = 0;

		TIME_LOOP("nn_mlp_64_32_16_4", 20, "one inference; 2,624 multiply-adds, 2,676 weights", chk += mlp_infer(nn_in));
		printf("I,nn_mlp_checksum,%d\n", chk);
		chk = 0;
		TIME_LOOP("nn_cnn_1d_128", 5, "one inference; 20,608 multiply-adds, 2,460 weights", chk += cnn_infer(nn_in));
		printf("I,nn_cnn_checksum,%d\n", chk);
	}
#ifdef BENCH_SMALL_RAM
	nn_in = NULL;
	cnn_a1 = NULL;
	cnn_a2 = NULL;
#endif
}
#endif /* BENCH_COREMARK_ONLY */

int main(void)
{
	uint32_t x = 1, q = 0;
	volatile uint32_t num = 1000000007U, den = 7U;
	volatile float f = 1.0f;
	size_t unused = 0;

	printf("\nBENCH START\n");
	printf("I,board,%s\n", BENCH_BOARD);
	printf("I,optimization,%s\n", BENCH_OPT);
	printf("I,compiler,GCC %s\n", __VERSION__);
	#ifdef BENCH_CC23
	printf("I,timer_hz,%u\n", BENCH_TIMER_HZ);
	printf("I,timer_hz_zephyr_config,%u,k_cycle_get_32 really counts 1 MHz; timed with SYSTIM TIME250N, 12 CPU cycles per tick\n",
	       sys_clock_hw_cycles_per_sec());
#else
	printf("I,timer_hz,%u\n", sys_clock_hw_cycles_per_sec());
#endif
	printf("I,cpu_hz_devicetree,%u\n", DT_PROP(DT_PATH(cpus, cpu_0), clock_frequency));
	printf("I,coremark_total_data_size,%u%s\n", (unsigned int)(TOTAL_DATA_SIZE),
	       (TOTAL_DATA_SIZE) == 2000 ? "" : ",not comparable with standard 2000-byte CoreMark scores");
#ifdef BENCH_COREMARK_ONLY
	printf("I,coremark_only,1,the other tests are compiled out to make room\n");
	ARG_UNUSED(x);
	ARG_UNUSED(q);
	ARG_UNUSED(num);
	ARG_UNUSED(den);
	ARG_UNUSED(f);
#else

	/* cost of one timer read, and of an empty loop */
	TIME_LOOP("timer_read", 1000, "one timer read; every timed interval includes about one of these", sink = bench_now());
	TIME_LOOP("empty_loop", 10000, "loop overhead; subtract from the rows below", __asm__ volatile("nop"));

	/* integer math */
	TIME_LOOP("mul32_add", 10000, "x = x * 1664525 + 1013904223", x = x * 1664525U + 1013904223U);
	sink = x;
	TIME_LOOP("udiv32_software", 2000, "1000000007 / (7 + i); the core has no divide instruction",
		  q += num / (den + i));
	sink = q;

	/* hardware math accelerator */
#ifndef BENCH_MATHACL
	printf("I,mathacl,not on this chip\n");
#else
	DL_MathACL_enablePower(MATHACL);
	k_busy_wait(100);
	printf("I,mathacl_div_check,%u,expect 142857143\n", hw_div(1000000007U, 7U));
	printf("I,mathacl_sincos_45deg,0x%08x,expect about 0x5a8... xor 0x5a7...\n", hw_sincos(0x20000000U));
	q = 0;
	TIME_LOOP("udiv32_mathacl", 2000, "same divide through the math accelerator", q += hw_div(num, den + i));
	sink = q;
	q = 0;
	TIME_LOOP("sincos_mathacl", 2000, "sine and cosine together, Q31, 10 iterations",
		  q += hw_sincos(i << 18));
	sink = q;
#endif

	/* software floating point: there is no FPU */
	TIME_LOOP("float_mul_add_software", 2000, "f = f * 1.0001f + 0.5f", f = f * 1.0001f + 0.5f);
	f = 2.0f;
	TIME_LOOP("sqrtf_software", 500, "sqrtf(float)", f = sqrtf(f + (float)i));
	f = 0.0f;
	TIME_LOOP("sinf_cosf_software", 200, "sinf plus cosf of a float", f += sinf(0.01f * (float)i) + cosf(0.01f * (float)i));
	sink = (uint32_t)f;

	/* memory */
	bench_memcpy();

	/* GPIO: led0 (see BENCH_LED_NAME) */
	gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	TIME_LOOP("gpio_toggle_zephyr_api", 2000, "gpio_pin_toggle_dt on the led0 pin", gpio_pin_toggle_dt(&led));
#ifdef BENCH_CC23
	/* GPIO_O_DOUTTGL31_0 (hw_gpio.h, 0x230): write a 1 to toggle that DIO. led0 is DIO15. */
	TIME_LOOP("gpio_toggle_register", 2000, "direct write to GPIO DOUTTGL31_0, DIO15",
		  HWREG(GPIO_BASE + GPIO_O_DOUTTGL31_0) = BIT(DT_GPIO_PIN(DT_ALIAS(led0), gpios)));
#elif defined(CONFIG_SOC_MSPM0L1306)
	TIME_LOOP("gpio_toggle_register", 2000, "direct write to the toggle register, PA0",
		  DL_GPIO_togglePins(GPIOA, BIT(DT_GPIO_PIN(DT_ALIAS(led0), gpios))));
#else
	TIME_LOOP("gpio_toggle_register", 2000, "direct write to the toggle register, PB22",
		  DL_GPIO_togglePins(GPIOB, DL_GPIO_PIN_22));
#endif
	gpio_pin_set_dt(&led, 0);

	/* interrupt latency: pend a spare interrupt, time to the first line of the handler */
	IRQ_CONNECT(SPARE_IRQ, 1, spare_isr, NULL, 0);
	irq_enable(SPARE_IRQ);
	{
		uint32_t best = UINT32_MAX, worst = 0;

		for (int i = 0; i < 200; i++) {
			uint32_t t0 = bench_now();

			NVIC_SetPendingIRQ(SPARE_IRQ);
			__ISB();
			uint32_t dt = isr_cycle - t0;

			best = MIN(best, dt);
			worst = MAX(worst, dt);
		}
		report("irq_latency_best", best, 1, "pend to first line of handler, through the Zephyr interrupt wrapper; includes about one timer_read");
		report("irq_latency_worst_of_200", worst, 1, "same, worst case seen");
	}
	irq_disable(SPARE_IRQ);

	/* thread switch: two threads handing the CPU back and forth */
	k_thread_create(&peer_thread, peer_stack, K_THREAD_STACK_SIZEOF(peer_stack), peer_entry, NULL, NULL,
			NULL, k_thread_priority_get(k_current_get()), 0, K_NO_WAIT);
	TIME_LOOP("thread_yield_round_trip", 1000, "k_yield to another thread and back; two switches", k_yield());
	k_thread_stack_space_get(&peer_thread, &unused);
	printf("I,peer_stack_bytes,%u\n", (unsigned int)BENCH_PEER_STACK);
	printf("I,peer_stack_unused,%u\n", (unsigned int)unused);
	k_thread_abort(&peer_thread);

	/* small neural nets in software */
	bench_nn();
#endif /* BENCH_COREMARK_ONLY */

	k_thread_stack_space_get(k_current_get(), &unused);
	printf("I,main_stack_bytes,%u\n", (unsigned int)CONFIG_MAIN_STACK_SIZE);
	printf("I,main_stack_unused_before_coremark,%u\n", (unsigned int)unused);

	printf("COREMARK START\n");
	coremark_main();
	k_thread_stack_space_get(k_current_get(), &unused);
	printf("I,main_stack_unused_after_coremark,%u\n", (unsigned int)unused);
	report_stacks();
	printf("BENCH DONE\n");
	return 0;
}
