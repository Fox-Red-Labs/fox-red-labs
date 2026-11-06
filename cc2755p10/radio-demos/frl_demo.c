/*
 *  Fox Red Labs radio-board demos: one source, three programs chosen at build time (-DFRL_DEMO=1 blink, 2 message, 3 button).
 *  Built on TI's drivers/empty and uart2echo examples (BSD-3-Clause, see NOTICE.txt beside each image).
 *
 *  Two blocks the Digital Lab page uses:
 *   the settings table at FRL_TABLE_ADDR: the shared parameter block (format 1); the page rewrites that one sector, no build
 *   frl_status  RAM: magic, demo, counter (rises every toggle, line or press), aux (buttons: bit 0 = BTN-1 down, bit 1 = BTN-2 down); the page reads it through the debug port
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include <ti/drivers/GPIO.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/UART2.h>

#include "ti_drivers_config.h"

#ifndef FRL_DEMO
#define FRL_DEMO 1
#endif
#define FRL_CONSOLE_BAUD 115200

#define PARAM_MAGIC  0x50524C46u   /* "FLRP": the shared parameter block, format 1 (tools/webflash/params.py) */
#define STATUS_MAGIC 0x53524C46u   /* "FLRS" */

#ifndef FRL_TABLE_ADDR
#define FRL_TABLE_ADDR 0x000DF800u /* the last 2 kB sector below TI's security firmware (CC27xx); 0x0007F800 on the CC2340R5 */
#endif
#define IMAGE_ID (5u + FRL_DEMO)   /* 6 blink, 7 message, 8 button */

typedef struct {
    uint32_t magic;
    uint32_t demo;
    uint32_t counter;
    uint32_t aux;
} FrlStatus;

__attribute__((used)) volatile FrlStatus frl_status = { STATUS_MAGIC, FRL_DEMO, 0, 0 };

/* The page writes one sector at FRL_TABLE_ADDR: words magic, format 1, count, image id, then one value per parameter.
 * Blank flash, a block for another image or a value outside its limits give the default. */
static uint32_t frl_get(uint32_t i, uint32_t def, uint32_t lo, uint32_t hi)
{
    const volatile uint32_t *b = (const volatile uint32_t *)FRL_TABLE_ADDR;
    if (b[0] == PARAM_MAGIC && b[1] == 1u && b[3] == IMAGE_ID && i < b[2] && b[2] <= 64u) {
        uint32_t v = b[4u + i];
        return v < lo ? lo : (v > hi ? hi : v);
    }
    return def;
}

/* parameter 0: blink on_ms, message period_ms, button repeat_ms; parameter 1 (blink only): off_ms */
#if FRL_DEMO == 1
#define P0() frl_get(0, 500u, 10u, 60000u)
#define P1() frl_get(1, 500u, 10u, 60000u)
#elif FRL_DEMO == 2
#define P0() frl_get(0, 1000u, 50u, 60000u)
#else
#define P0() frl_get(0, 1000u, 100u, 60000u)
#endif
static const char kText[] = "Hello from my LaunchPad";

static UART2_Handle uart;

static void say(const char *s)
{
    size_t n = 0;
    UART2_write(uart, s, strlen(s), &n);
}

static char *putnum(char *p, uint32_t v)
{
    char t[10];
    int i = 0;
    do { t[i++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (i) *p++ = t[--i];
    return p;
}

static void openConsole(void)
{
    UART2_Params up;
    UART2_Params_init(&up);
    up.baudRate = FRL_CONSOLE_BAUD;
    uart = UART2_open(CONFIG_UART2_0, &up);
    /* A blocking write returns when the DMA is done, before the UART's FIFO has emptied: standby in the next usleep() would cut the last bytes of a line. */
    Power_setConstraint(PowerLPF3_DISALLOW_STANDBY);
}

void *mainThread(void *arg0)
{
    GPIO_init();
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_setConfig(CONFIG_GPIO_LED_1, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

#if FRL_DEMO == 1
    while (1) {
        GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);
        frl_status.counter++;
        usleep(P0() * 1000u);
        GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
        frl_status.counter++;
        usleep(P1() * 1000u);
    }
#elif FRL_DEMO == 2
    openConsole();
    while (1) {
        char line[64], *p = line;
        const char *t = kText;
        *p++ = '>'; *p++ = ' ';
        for (int i = 0; t[i]; i++) *p++ = t[i];
        *p++ = ' ';
        frl_status.counter++;
        p = putnum(p, frl_status.counter);
        *p++ = '\r'; *p++ = '\n'; *p = 0;
        GPIO_toggle(CONFIG_GPIO_LED_0);
        say(line);
        usleep(P0() * 1000u);
    }
#else
    GPIO_setConfig(CONFIG_GPIO_BUTTON_0, GPIO_CFG_IN_PU);
    GPIO_setConfig(CONFIG_GPIO_BUTTON_1, GPIO_CFG_IN_PU);
    openConsole();
    uint32_t last = 0xFFu, beat = 0;
    while (1) {
        /* the buttons are active low: down reads 0 */
        uint32_t now = (GPIO_read(CONFIG_GPIO_BUTTON_0) ? 0u : 1u) | (GPIO_read(CONFIG_GPIO_BUTTON_1) ? 0u : 2u);
        GPIO_write(CONFIG_GPIO_LED_0, (now & 1u) ? CONFIG_GPIO_LED_ON : CONFIG_GPIO_LED_OFF);
        GPIO_write(CONFIG_GPIO_LED_1, (now & 2u) ? CONFIG_GPIO_LED_ON : CONFIG_GPIO_LED_OFF);
        frl_status.aux = now;
        if (now != last) {
            if ((now & ~last) != 0u) frl_status.counter++;   /* a press: a button that was up and is down */
            char line[64], *p = line;
            const char *s = "buttons: BTN-1 ";
            while (*s) *p++ = *s++;
            s = (now & 1u) ? "down" : "up"; while (*s) *p++ = *s++;
            s = ", BTN-2 "; while (*s) *p++ = *s++;
            s = (now & 2u) ? "down" : "up"; while (*s) *p++ = *s++;
            s = ", presses "; while (*s) *p++ = *s++;
            p = putnum(p, frl_status.counter);
            *p++ = '\r'; *p++ = '\n'; *p = 0;
            say(line);
            last = now;
            beat = 0;
        }
        usleep(10000);
        if (++beat >= P0() / 10u) {
            beat = 0;
            last = 0xFFu;   /* print the state again once per period, so the console is never silent */
        }
    }
#endif
}
