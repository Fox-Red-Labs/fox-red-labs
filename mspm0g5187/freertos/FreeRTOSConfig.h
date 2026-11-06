/*
 * FreeRTOSConfig.h for the LP-MSPM0G5187 (Cortex-M0+, 32 MHz, 64 kB flash, 4 kB SRAM).
 *
 * Starting point: TI's kernel/freertos/builds/LP_MSPM0L1306/release/FreeRTOSConfig.h from
 * MSPM0 SDK 2.11.00.07 (FreeRTOS Kernel V11.2.0, MIT license). Same clock, tick rate, preemption,
 * interrupt priorities and port. Changed to the smallest set that still runs tasks, delays,
 * semaphores, queues and task notifications; every change is listed in README.md.
 *
 * Build-time knobs (build.sh passes them with -D):
 *   FR_MIN_STACK_WORDS  configMINIMAL_STACK_SIZE in 32-bit words: the idle task's stack and the
 *                       "minimal task" stack in the edge test. TI's file uses 128 (512 bytes).
 *   FR_HEAP_BYTES       0: static allocation only, no heap. >0: heap_4 with this many bytes,
 *                       dynamic allocation only (used by the edge-heap image).
 *   FR_SAFETY           1: configASSERT and stack overflow check method 2 (TI's file has both).
 *                       0: neither, to measure what they cost (bench-lean image).
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#ifndef FR_MIN_STACK_WORDS
#define FR_MIN_STACK_WORDS 48
#endif
#ifndef FR_HEAP_BYTES
#define FR_HEAP_BYTES 0
#endif
#ifndef FR_CPU_HZ
#define FR_CPU_HZ 32000000
#endif
#ifndef FR_SAFETY
#define FR_SAFETY 1
#endif

/* Clock and tick: same as TI's file. SysTick runs from the 32 MHz CPU clock. */
#define configCPU_CLOCK_HZ                      ((unsigned long) FR_CPU_HZ)
#define configTICK_RATE_HZ                      1000
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0 /* TI: 1 */
#define configMAX_PRIORITIES                    3 /* TI: 10. 20 bytes of RAM per level */
#define configMINIMAL_STACK_SIZE                ((unsigned short) FR_MIN_STACK_WORDS) /* TI: 128 */
#define configMAX_TASK_NAME_LEN                 4 /* TI: 16. 4 keeps the task block at 64 bytes */
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                 0
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1
#define configQUEUE_REGISTRY_SIZE               0
#define configENABLE_BACKWARD_COMPATIBILITY     0
#define configUSE_MINI_LIST_ITEM                1
#define configSTACK_DEPTH_TYPE                  size_t
#define configMESSAGE_BUFFER_LENGTH_TYPE        size_t
#define configENABLE_MPU                        0

/* Kernel features: TI's file turns these on; none is needed for tasks, delays, semaphores,
 * queues and notifications. Software timers would add a timer task and its queue. */
#define configUSE_TIMERS                0 /* TI: 1 */
#define configUSE_EVENT_GROUPS          0 /* TI: 1 */
#define configUSE_STREAM_BUFFERS        0 /* TI: 1 */
#define configUSE_MUTEXES               0 /* TI: 1. 8 bytes per task block */
#define configUSE_RECURSIVE_MUTEXES     0 /* TI: 1 */
#define configUSE_COUNTING_SEMAPHORES   0 /* TI: 1 */
#define configUSE_QUEUE_SETS            0
#define configUSE_TASK_NOTIFICATIONS    1
#define configUSE_APPLICATION_TASK_TAG  0 /* TI: 1 */
#define configUSE_TRACE_FACILITY        0 /* TI: 1 */
#define configUSE_STATS_FORMATTING_FUNCTIONS 0
#define configGENERATE_RUN_TIME_STATS   0
#define configUSE_CO_ROUTINES           0
#define configMAX_CO_ROUTINE_PRIORITIES 1
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 0 /* TI: 1 or 2, for its POSIX layer */

/* Memory. TI's file allows both kinds with a 3,072-byte heap_4 heap. */
#if FR_HEAP_BYTES > 0
#define configSUPPORT_STATIC_ALLOCATION  0
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configTOTAL_HEAP_SIZE            ((size_t) FR_HEAP_BYTES)
#else
#define configSUPPORT_STATIC_ALLOCATION  1
#define configSUPPORT_DYNAMIC_ALLOCATION 0
#define configTOTAL_HEAP_SIZE            ((size_t) 0)
#endif
#define configAPPLICATION_ALLOCATED_HEAP          0
#define configSTACK_ALLOCATION_FROM_SEPARATE_HEAP 0
#define configENABLE_HEAP_PROTECTOR               0
#define configHEAP_CLEAR_MEMORY_ON_FREE           0
#define configKERNEL_PROVIDED_STATIC_MEMORY       1 /* kernel supplies the idle task's block and stack */

/* Interrupt priorities: same as TI's file. 2 priority bits; kernel (SysTick, PendSV) lowest. */
#define configPRIO_BITS                              2
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      0x03
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 1
#define configKERNEL_INTERRUPT_PRIORITY      (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* Hooks */
#define configUSE_IDLE_HOOK              0
#define configUSE_TICK_HOOK              0
#define configUSE_MALLOC_FAILED_HOOK     0
#define configUSE_DAEMON_TASK_STARTUP_HOOK 0
#define configUSE_SB_COMPLETED_CALLBACK  0

#if FR_SAFETY
#define configCHECK_FOR_STACK_OVERFLOW   2
/* On failure: interrupts off, record the line in fr_fault (fr_common.c), stop. */
void fr_assert_failed(unsigned int line);
#define configASSERT(x) if ((x) == 0) { fr_assert_failed(__LINE__); }
#define configCHECK_HANDLER_INSTALLATION 1
#else
#define configCHECK_FOR_STACK_OVERFLOW   0
#define configCHECK_HANDLER_INSTALLATION 0
#endif

/* API functions compiled in */
#define INCLUDE_vTaskPrioritySet            0
#define INCLUDE_uxTaskPriorityGet           0
#define INCLUDE_vTaskDelete                 0
#define INCLUDE_vTaskSuspend                1 /* portMAX_DELAY then waits forever */
#define INCLUDE_xResumeFromISR              0
#define INCLUDE_xTaskDelayUntil             0
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_xTaskGetSchedulerState      0
#define INCLUDE_xTaskGetCurrentTaskHandle   0
#define INCLUDE_uxTaskGetStackHighWaterMark 1 /* TI: 0. Needed for the stack reports */
#define INCLUDE_xTaskGetIdleTaskHandle      1 /* TI: 0. To read the idle task's stack */
#define INCLUDE_eTaskGetState               0
#define INCLUDE_xEventGroupSetBitFromISR    0
#define INCLUDE_xTimerPendFunctionCall      0
#define INCLUDE_xTaskAbortDelay             0
#define INCLUDE_xTaskGetHandle              0
#define INCLUDE_xTaskResumeFromISR          0
#define INCLUDE_xSemaphoreGetMutexHolder    0

#endif /* FREERTOS_CONFIG_H */
