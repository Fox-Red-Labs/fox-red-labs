/* cl2000's C library has no strcasecmp; board.c supplies it. uint8_t is a 16-bit unsigned char here (C28x char is 16 bits). */
#include <stdint.h>
#include <limits.h>
/* int is 16 bits here; the sketch builds with every int widened to long, so its overflow limits are the 32-bit ones */
#undef INT_MAX
#undef INT_MIN
#define INT_MAX 2147483647L
#define INT_MIN (-2147483647L-1)
typedef unsigned char uint8_t;
typedef signed char int8_t;
#ifdef __cplusplus
extern "C" {
#endif
int strcasecmp(const char *a, const char *b);
#ifdef __cplusplus
}
#endif
