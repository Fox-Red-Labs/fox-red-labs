/* TI CC35xx shim, seen only by ESPHome's C++ (the build puts ESPHome's src/ ahead of TI's lwIP port on the include path).
 * TI's lwipopts.h sets LWIP_COMPAT_SOCKETS 1, which turns connect(), close() and friends into macros and breaks
 * ESPHome's socket class methods of the same names. The setting only adds aliases; the library's structures are the same. */
#include_next "lwipopts.h"
#undef LWIP_COMPAT_SOCKETS
#define LWIP_COMPAT_SOCKETS 0
