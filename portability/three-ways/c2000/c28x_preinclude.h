/* The C28x has no 8-bit type, so the compiler's stdint.h leaves out uint8_t. app.h uses it. Pass this with
 * --preinclude so common/app.c and app.h stay unchanged. uint8_t is then a 16-bit unsigned char. */
#ifndef C28X_PREINCLUDE_H
#define C28X_PREINCLUDE_H
#include <stdint.h>
typedef unsigned char uint8_t;
#endif
