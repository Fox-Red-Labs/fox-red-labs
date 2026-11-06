/* Build options for libopus 1.5.2 in this port. One of OPUS_PORT_FIXED or OPUS_PORT_FLOAT is set on the compiler command line.
 * No lrint: float to integer conversion uses the portable floor(0.5 + x) on the Mac and on the board, so both round alike. */
#ifndef OPUS_PORT_CONFIG_H
#define OPUS_PORT_CONFIG_H
#define OPUS_BUILD 1
#define PACKAGE_VERSION "1.5.2"
#define VAR_ARRAYS 1
#if defined(OPUS_PORT_FIXED)
#define FIXED_POINT 1
#define DISABLE_FLOAT_API 1
#elif defined(OPUS_PORT_FLOAT)
#define FLOATING_POINT 1
#else
#error "define OPUS_PORT_FIXED or OPUS_PORT_FLOAT"
#endif
#endif
