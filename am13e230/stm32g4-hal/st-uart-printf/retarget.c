/* printf to the UART: TI's C library calls write(); ST's example defines __io_putchar (for GNU libc) and calls
   HAL_UART_Transmit with a 0xFFFF timeout, one character at a time. Send each character through ST's function. */
extern int __io_putchar(int ch);
int write(int fd, const char *buf, unsigned len)
{
    (void)fd;
    for (unsigned i = 0; i < len; i++) __io_putchar(buf[i]);
    return (int)len;
}
/* TI's C library buffers stdout when it is not a terminal and the example never returns from main, so nothing would
   flush: turn the buffering off before main runs. */
#include <stdio.h>
__attribute__((constructor)) static void unbuffer_stdout(void) { setvbuf(stdout, 0, _IONBF, 0); }
