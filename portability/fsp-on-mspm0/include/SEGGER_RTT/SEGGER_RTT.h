/* Stand-in for the SEGGER RTT header the ra-fsp-examples include: the debug console goes to the board's UART0 (probe backchannel). */
#ifndef SEGGER_RTT_H_
#define SEGGER_RTT_H_
#include <stdarg.h>
int SEGGER_RTT_printf(unsigned BufferIndex, const char *sFormat, ...);
unsigned SEGGER_RTT_Read(unsigned BufferIndex, void *pBuffer, unsigned BufferSize);   /* non-blocking, from UART0 */
int SEGGER_RTT_HasKey(void);
#endif
