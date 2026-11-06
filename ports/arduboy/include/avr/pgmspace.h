/* Flat-memory stand-in for AVR <avr/pgmspace.h>: const data already lives in flash on ARM. */
#ifndef FRL_AVR_PGMSPACE_H
#define FRL_AVR_PGMSPACE_H
#include <stdint.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
#ifndef PROGMEM
#define PROGMEM
#endif
#define PSTR(s) (s)
#define pgm_read_byte(a)  (*(const uint8_t *)(a))
#define pgm_read_byte_near(a) pgm_read_byte(a)
static inline uint16_t frl_rd16(const void *a) { uint16_t v; memcpy(&v, a, 2); return v; }
static inline uint32_t frl_rd32(const void *a) { uint32_t v; memcpy(&v, a, 4); return v; }
static inline void *frl_rdptr(const void *a) { void *v; memcpy(&v, a, sizeof v); return v; }
#define pgm_read_word(a)  frl_rd16(a)
#define pgm_read_word_near(a) frl_rd16(a)
#define pgm_read_dword(a) frl_rd32(a)
#define pgm_read_ptr(a)   frl_rdptr(a)
#define memcpy_P(d, s, n) memcpy((d), (s), (n))
#define strlen_P(s) strlen(s)
#define strcpy_P(d, s) strcpy((d), (s))
#define strcmp_P(a, b) strcmp((a), (b))
#ifdef __cplusplus
}
#endif
#endif
