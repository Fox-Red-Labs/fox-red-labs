/* The C28x has no 8-bit type (CHAR_BIT is 16) and TI's stdint.h leaves out int8_t and uint8_t.
   These typedefs make them 16-bit. Values the occupancy models store are always clamped to
   -128..127 (or 0..255 for shifts), so results match 8-bit machines; storage doubles. */
#ifndef C28_INT8_H
#define C28_INT8_H
#include <stdint.h>
#ifndef __int8_t_defined
#define __int8_t_defined
typedef int16_t int8_t;
typedef uint16_t uint8_t;
#endif
#endif
