/*
 * Compile shim for TI's CC1407P host driver on the C28x (cl2000). Pass it with --preinclude.
 * It changes none of TI's files and adds no run-time code. It does two things:
 *   1. Gives the exact-width 8-bit names the C28x stdint.h leaves out. A C28x "char" is 16 bits, so these
 *      are 16-bit unsigned char / signed char: the type the driver's byte arrays need to be valid C.
 *      (C2000Ware's "typedef uint16_t uint8_t;" also compiles the driver, but breaks u8g2-style const char tables.)
 *   2. Turns TRX.h's __attribute__((__packed__)) into an empty attribute. The C2000 compiler rejects it
 *      (error #1167) and every C28x member is already a whole word, so packing has no meaning there.
 * It does NOT make the message structs match the SPI wire bytes (see c28x/README.md).
 */
#ifndef FRL_C28X_COMPAT_H
#define FRL_C28X_COMPAT_H
#ifdef __TMS320C2000__
#include <stdint.h>
typedef unsigned char uint8_t;
typedef signed char int8_t;
#define __packed__
#endif
#endif
