#ifndef SHIM_SEM_H
#define SHIM_SEM_H
#include "pico.h"
/* Single-core stand-in: counting semaphores that never block. A wait on an empty one stops the program. */
typedef struct semaphore { volatile int16_t permits; int16_t max_permits; } semaphore_t;
#ifdef __cplusplus
extern "C" {
#endif
void shim_sem_stuck(semaphore_t *s);   /* runs the other core's loop once, or stops */
void shim_sem_released(semaphore_t *s);
#ifdef __cplusplus
}
#endif
static inline void sem_init(semaphore_t *s, int16_t initial, int16_t max) { s->permits = initial; s->max_permits = max; }
static inline bool sem_release(semaphore_t *s) { bool r = false; if (s->permits < s->max_permits) { s->permits++; r = true; } shim_sem_released(s); return r; }
static inline void sem_reset(semaphore_t *s, int16_t p) { s->permits = p; }
static inline int sem_available(semaphore_t *s) { return s->permits; }
static inline bool sem_try_acquire(semaphore_t *s) { if (s->permits > 0) { s->permits--; return true; } return false; }
static inline void sem_acquire_blocking(semaphore_t *s) { if (!sem_try_acquire(s)) { shim_sem_stuck(s); if (!sem_try_acquire(s)) __breakpoint(); } }
static inline bool sem_acquire_timeout_ms(semaphore_t *s, uint32_t ms) { (void)ms; return sem_try_acquire(s); }
#endif
