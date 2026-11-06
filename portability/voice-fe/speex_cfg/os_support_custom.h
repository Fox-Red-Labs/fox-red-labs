/* SpeexDSP allocation and message hooks for portability/voice-fe. Every allocation comes from one static arena
 * (fe_arena.c) that never frees, so the high-water mark is the front end's whole heap. Messages are counted, not printed. */
#ifndef VOICEFE_OS_SUPPORT_CUSTOM_H
#define VOICEFE_OS_SUPPORT_CUSTOM_H
#include <stddef.h>
void *fe_arena_alloc(int size);       /* zeroed */
extern volatile unsigned fe_speex_msgs;
extern volatile unsigned fe_speex_fatal;
#define OVERRIDE_SPEEX_ALLOC
static inline void *speex_alloc(int size) { return fe_arena_alloc(size); }
#define OVERRIDE_SPEEX_ALLOC_SCRATCH
static inline void *speex_alloc_scratch(int size) { return fe_arena_alloc(size); }
#define OVERRIDE_SPEEX_REALLOC
static inline void *speex_realloc(void *ptr, int size) { (void)ptr; (void)size; fe_speex_fatal++; return 0; }
#define OVERRIDE_SPEEX_FREE
static inline void speex_free(void *ptr) { (void)ptr; }
#define OVERRIDE_SPEEX_FREE_SCRATCH
static inline void speex_free_scratch(void *ptr) { (void)ptr; }
#define OVERRIDE_SPEEX_FATAL
static inline void _speex_fatal(const char *str, const char *file, int line) { (void)str; (void)file; (void)line; fe_speex_fatal++; for (;;) { } }
#define OVERRIDE_SPEEX_WARNING
static inline void speex_warning(const char *str) { (void)str; fe_speex_msgs++; }
#define OVERRIDE_SPEEX_WARNING_INT
static inline void speex_warning_int(const char *str, int val) { (void)str; (void)val; fe_speex_msgs++; }
#define OVERRIDE_SPEEX_NOTIFY
static inline void speex_notify(const char *str) { (void)str; fe_speex_msgs++; }
#define OVERRIDE_SPEEX_PUTC
static inline void _speex_putc(int ch, void *file) { (void)ch; (void)file; }
#endif
