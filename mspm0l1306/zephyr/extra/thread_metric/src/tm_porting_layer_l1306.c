/*
 * Thread-Metric porting layer for Zephyr, sized for 4 KB of RAM.
 * Derived from Zephyr's tests/benchmarks/thread_metric/src/tm_porting_layer_zephyr.c
 * (Copyright (c) 2024 Microsoft Corporation, Intel Corporation; MIT license).
 * Changes: thread count and stack size come from the build (TM_TEST_NUM_THREADS, TM_TEST_STACK_SIZE),
 * one semaphore, one queue, one slab of two blocks (no test uses more), and each reporting period
 * prints unused stack bytes (lines starting "S,") before the test prints its own result.
 */
#include "tm_api.h"

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/irq_offload.h>

/* Kernel internals, declared here the same way mcu-bench-l1306 does (the header is private). */
extern struct k_thread z_idle_threads[];
K_KERNEL_STACK_ARRAY_DECLARE(z_interrupt_stacks, CONFIG_MP_MAX_NUM_CPUS, CONFIG_ISR_STACK_SIZE);
int z_stack_space_get(const uint8_t *stack_start, size_t size, size_t *unused_ptr);

#if (CONFIG_MP_MAX_NUM_CPUS > 1)
#error "*** Tests are only designed for single processor systems! ***"
#endif

static struct k_thread test_thread[TM_TEST_NUM_THREADS];
static K_THREAD_STACK_ARRAY_DEFINE(test_stack, TM_TEST_NUM_THREADS, TM_TEST_STACK_SIZE);
static struct k_sem test_sem[1];
static struct k_msgq test_msgq[1];
static char test_msgq_buffer[8][16];
static struct k_mem_slab test_slab[1];
static char __aligned(4) test_slab_buffer[2 * 128];

void tm_initialize(void (*test_initialization_function)(void))
{
	test_initialization_function();
}

int tm_thread_create(int thread_id, int priority, void (*entry_function)(void *, void *, void *))
{
	k_tid_t tid;

	tid = k_thread_create(&test_thread[thread_id], test_stack[thread_id],
			      TM_TEST_STACK_SIZE, entry_function,
			      NULL, NULL, NULL, priority, 0, K_FOREVER);
	k_thread_suspend(&test_thread[thread_id]);
	k_wakeup(&test_thread[thread_id]);
	return (tid == &test_thread[thread_id]) ? TM_SUCCESS : TM_ERROR;
}

int tm_thread_resume(int thread_id)
{
	k_thread_resume(&test_thread[thread_id]);
	return TM_SUCCESS;
}

int tm_thread_suspend(int thread_id)
{
	k_thread_suspend(&test_thread[thread_id]);
	return TM_SUCCESS;
}

void tm_thread_relinquish(void)
{
	k_yield();
}

static void print_stacks(void)
{
	size_t unused;
	int i;

	for (i = 0; i < TM_TEST_NUM_THREADS; i++) {
		if (k_thread_stack_space_get(&test_thread[i], &unused) == 0) {
			printf("S,test_thread_%d_unused_of_%d,%u\n", i, TM_TEST_STACK_SIZE,
			       (unsigned int)unused);
		}
	}
	k_thread_stack_space_get(k_current_get(), &unused);
	printf("S,main_unused_of_%d,%u\n", CONFIG_MAIN_STACK_SIZE, (unsigned int)unused);
	z_stack_space_get(K_KERNEL_STACK_BUFFER(z_interrupt_stacks[0]),
			  K_KERNEL_STACK_SIZEOF(z_interrupt_stacks[0]), &unused);
	printf("S,isr_unused_of_%d,%u\n", CONFIG_ISR_STACK_SIZE, (unsigned int)unused);
	k_thread_stack_space_get(&z_idle_threads[0], &unused);
	printf("S,idle_unused_of_%d,%u\n", CONFIG_IDLE_STACK_SIZE, (unsigned int)unused);
}

/* Called by each test's reporting loop (on the main thread) once per period. */
void tm_thread_sleep(int seconds)
{
	k_sleep(K_SECONDS(seconds));
	print_stacks();
}

int tm_queue_create(int queue_id)
{
	k_msgq_init(&test_msgq[0], &test_msgq_buffer[0][0], 16, 8);
	return TM_SUCCESS;
}

int tm_queue_send(int queue_id, unsigned long *message_ptr)
{
	return k_msgq_put(&test_msgq[0], message_ptr, K_FOREVER);
}

int tm_queue_receive(int queue_id, unsigned long *message_ptr)
{
	return k_msgq_get(&test_msgq[0], message_ptr, K_FOREVER);
}

int tm_semaphore_create(int semaphore_id)
{
	return k_sem_init(&test_sem[0], 1, 1);
}

int tm_semaphore_get(int semaphore_id)
{
	return k_sem_take(&test_sem[0], K_NO_WAIT);
}

int tm_semaphore_put(int semaphore_id)
{
	k_sem_give(&test_sem[0]);
	return TM_SUCCESS;
}

extern void tm_interrupt_handler(const void *);

void tm_cause_interrupt(void)
{
	irq_offload(tm_interrupt_handler, NULL);
}

int tm_memory_pool_create(int pool_id)
{
	return (k_mem_slab_init(&test_slab[0], &test_slab_buffer[0], 128, 2) == 0) ? TM_SUCCESS : TM_ERROR;
}

int tm_memory_pool_allocate(int pool_id, unsigned char **memory_ptr)
{
	return (k_mem_slab_alloc(&test_slab[0], (void **)memory_ptr, K_NO_WAIT) == 0) ? TM_SUCCESS : TM_ERROR;
}

int tm_memory_pool_deallocate(int pool_id, unsigned char *memory_ptr)
{
	k_mem_slab_free(&test_slab[0], (void *)memory_ptr);
	return TM_SUCCESS;
}
