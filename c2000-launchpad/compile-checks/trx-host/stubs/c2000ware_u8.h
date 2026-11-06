/* Mode "c2000ware": the 8-bit names C2000Ware's driverlib gives (hw_types.h line 80:
 * "typedef uint16_t uint8_t;"), so the driver sees the types a C2000Ware project would hand it. Ours, not TI's. */
#include <stdint.h>
typedef uint16_t uint8_t;
typedef int16_t int8_t;
