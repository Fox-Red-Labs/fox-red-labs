/* Three ways, Linux glue: the five calls in app.h as plain C on Linux user space (sysfs, no libraries).
 * ADC: IIO sysfs; UART: a tty or stdout; LED: the LED class. Paths come from config_am62l.h. */
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "app.h"
#include "config_am62l.h"

static int uart_fd = 1;   /* stdout unless FRL_UART names a tty */
static int led_on;

static int sysfs_write(const char *path, const char *s, size_t n)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    ssize_t w = write(fd, s, n);
    close(fd);
    return w == (ssize_t)n ? 0 : -1;
}

void board_init(void)
{
    const char *tty = getenv("FRL_UART");
    if (tty != NULL) {
        uart_fd = open(tty, O_WRONLY | O_NOCTTY);
        if (uart_fd < 0) exit(2);
        struct termios t;
        if (tcgetattr(uart_fd, &t) == 0) {
            cfsetospeed(&t, B115200);
            t.c_oflag &= (tcflag_t)~OPOST;        /* raw bytes, as on a UART: no extra \r */
            tcsetattr(uart_fd, TCSANOW, &t);
        }
    }
    sysfs_write(LED_DIR "/trigger", "none", 4);   /* take the LED from its kernel trigger */
}

uint16_t adc_read(void)
{
    char b[16] = {0};
    int fd = open(ADC_RAW, O_RDONLY);
    if (fd < 0) return 0xFFFFu;
    ssize_t n = read(fd, b, sizeof b - 1);
    close(fd);
    return n > 0 ? (uint16_t)strtoul(b, NULL, 10) : 0xFFFFu;
}

void uart_write(const char *s, uint32_t len)
{
    while (len > 0u) {
        ssize_t w = write(uart_fd, s, len);
        if (w <= 0) return;
        s += w;
        len -= (uint32_t)w;
    }
}

void led_toggle(void)
{
    led_on = !led_on;
    sysfs_write(LED_DIR "/brightness", led_on ? "1" : "0", 1);
}

void sleep_ms(uint32_t ms)
{
    struct timespec ts = { (time_t)(ms / 1000u), (long)(ms % 1000u) * 1000000L };
    while (nanosleep(&ts, &ts) != 0) {
    }
}

int main(void)
{
    board_init();
    app_run();
}
