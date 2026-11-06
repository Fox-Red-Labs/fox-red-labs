/*
 * Where each Cmsemicon pin lands on the LP-MSPM0L1306. This is the configuration layer:
 * change a line here, nothing else.
 *
 * The CMS32F040 board the examples were written for has ports A, B, C and F. The
 * MSPM0L1306 has one port, A, pins PA0 to PA27. The rule below keeps the examples'
 * meaning (an LED, a key, an analog input) rather than the pin number.
 *
 * LP-MSPM0L1306 facts used (TI SysConfig output and the LaunchPad guide):
 *   LED1 red = PA0 (PINCM1); RGB LED2 red PA13, green PA26, blue PA27;
 *   S2 button = PA14 (pull-up, active low); UART0 to the USB port: TX PA8, RX PA9;
 *   ADC inputs A0 PA27, A1 PA26, A2 PA25, A3 PA24, A4 PA22, A5 PA21, A6 PA20, A7 PA18,
 *   A8 PA16, A9 PA15; TIMG0 PWM PA12 (C0), PA13 (C1, the red of the RGB LED);
 *   COMP0 output PA7; OPA0 IN0+ PA26, IN0- PA24, OUT PA22 (light sensor on the board);
 *   OPA1 IN0+ PA18, IN1- PA17, OUT PA16.
 */
#ifndef CMS_PINMAP_H
#define CMS_PINMAP_H

#include "CMS32F04xx.h"

/* Returns the GPIOA pin index (0..27) for a Cmsemicon (port, pin). */
static inline uint32_t cms_pin_index(PORT_TypeDef port, PIN_TypeDef pin)
{
    switch (port) {
    case PORTB:
        if (pin == PIN8) return 0;    /* the examples' LED on PB8      -> LED1, PA0        */
        if (pin == PIN6) return 0;    /* LED1 on PB6                   -> LED1, PA0        */
        if (pin == PIN7) return 27;   /* LED2 on PB7                   -> RGB blue, PA27   */
        if (pin == PIN0) return 0;    /* Blinky LED on PB0             -> PA0 (the deep-sleep key is wired in intp_demo.c) */
        if (pin == PIN1) return 1;    /* Blinky's second LED           -> PA1 (header pin) */
        if (pin == PIN4) return 3;    /* timer17 break input PB4       -> PA3              */
        return pin;
    case PORTF:
        if (pin == PIN3 || pin == PIN4) return 0;   /* LED on PF3/PF4 -> LED1, PA0 */
        if (pin == PIN0) return 0;    /* Gpio_input LEDs PF0, PF1      -> PA0, PA1        */
        if (pin == PIN1) return 1;
        return 20 + pin;              /* other PF pins -> PA20..        */
    case PORTC:
        return 15 + pin;              /* PC pins -> PA15..              */
    case PORTA:
    default:
        if (pin == PIN4) return 0;    /* uart example's LED on PA4     -> LED1, PA0        */
        if (pin == PIN0) return 14;   /* intp example's key on PA0     -> S2, PA14         */
        return pin;                   /* PA2, PA3, PA6, PA7, PA9, PA10, PA11: same number  */
    }
}

/* ADC channel numbers map one to one (AN2 -> A2 = PA25 and so on). */
static inline uint32_t cms_adc_chan(uint32_t ch) { return ch; }

#endif
