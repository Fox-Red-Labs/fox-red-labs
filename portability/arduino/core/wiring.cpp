/* GPIO, time and start-up for the Arduino compatibility layer on the LP-MSPM0G3507. */
#include "Arduino.h"
#include "pins_internal.h"
#include <stdlib.h>

/* ---------- pin table ---------- */
/* IOMUX_PINCMn number (1 based, from the G350x header and the data sheet pin table) for each GPIO.
   0 = the pin does not exist on this device. */
static const uint8_t pincm_a[32] = {
   1,  2,  7,  8,  9, 10, 11, 14,        /* PA0..PA7 */
  19, 20, 21, 22, 34, 35, 36, 37,        /* PA8..PA15 */
  38, 39, 40, 41, 42, 46, 47, 53,        /* PA16..PA23 */
  54, 55, 59, 60,  3,  4,  5,  6 };      /* PA24..PA31 */
static const uint8_t pincm_b[32] = {
  12, 13, 15, 16, 17, 18, 23, 24,        /* PB0..PB7 */
  25, 26, 27, 28, 29, 30, 31, 32,        /* PB8..PB15 */
  33, 43, 44, 45, 48, 49, 50, 51,        /* PB16..PB23 */
  52, 56, 57, 58,  0,  0,  0,  0 };      /* PB24..PB31 (PB28 to PB31 do not exist) */

volatile uint32_t compat_bad_pin_number;
extern "C" void compat_bad_pin(uint32_t pin)
{
  compat_bad_pin_number = pin;
  for (;;) __asm volatile("bkpt #0");
}

volatile const char *compat_unsupported_what;
extern "C" void compat_unsupported(const char *what)
{
  compat_unsupported_what = what;
  for (;;) __asm volatile("bkpt #1");
}

uint32_t pin_iomux(uint32_t pin)
{
  uint8_t n = 0;
  if (pin < 32) n = pincm_a[pin];
  else if (pin < NUM_DIGITAL_PINS + 4) n = pincm_b[pin - 32];
  if (n == 0) compat_bad_pin(pin);
  return n - 1;   /* IOMUX_PINCM1 is 0 */
}

/* ---------- time: SysTick, 1 ms ---------- */
static volatile uint32_t ms_ticks;
#define SYSTICK_LOAD (F_CPU / 1000 - 1)

extern "C" void SysTick_Handler(void) { ms_ticks++; }

unsigned long millis(void) { return ms_ticks; }

unsigned long micros(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uint32_t ms = ms_ticks;
  uint32_t v = SysTick->VAL;
  if (SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) {   /* wrapped, handler has not run yet */
    ms++;
    v = SysTick->VAL;
  }
  if (!primask) __enable_irq();
  return ms * 1000UL + (SYSTICK_LOAD - v) / (F_CPU / 1000000UL);
}

void delayMicroseconds(unsigned int us)
{
  if (us) delay_cycles((uint32_t)us * (F_CPU / 1000000UL));
}

void delay(unsigned long ms)
{
  uint32_t start = micros();
  while (ms > 0) {
    yield();
    while (ms > 0 && (micros() - start) >= 1000) { ms--; start += 1000; }
  }
}

extern "C" void yield(void) __attribute__((weak));
extern "C" void yield(void) {}

/* ---------- GPIO ---------- */
void pinMode(pin_size_t pin, PinMode mode)
{
  uint32_t idx = pin_iomux(pin);
  GPIO_Regs *port = pin_port(pin);
  uint32_t mask = pin_mask(pin);
  switch (mode) {
  case INPUT:
  case INPUT_PULLUP:
  case INPUT_PULLDOWN:
    DL_GPIO_initDigitalInputFeatures(idx, DL_GPIO_INVERSION_DISABLE,
        mode == INPUT_PULLUP ? DL_GPIO_RESISTOR_PULL_UP :
        mode == INPUT_PULLDOWN ? DL_GPIO_RESISTOR_PULL_DOWN : DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_disableOutput(port, mask);
    break;
  case OUTPUT:
    DL_GPIO_initDigitalOutput(idx);
    DL_GPIO_enableOutput(port, mask);
    break;
  case OUTPUT_OPENDRAIN:
    DL_GPIO_initDigitalOutput(idx);
    DL_GPIO_enableHiZ(idx);
    DL_GPIO_enableOutput(port, mask);
    break;
  }
}

void digitalWrite(pin_size_t pin, PinStatus status)
{
  if (pin >= NUM_DIGITAL_PINS) compat_bad_pin(pin);
  GPIO_Regs *port = pin_port(pin);
  uint32_t mask = pin_mask(pin);
  if (status == LOW) {
    DL_GPIO_clearPins(port, mask);
  } else {
    DL_GPIO_setPins(port, mask);
    if (pin == CS_GAP_PIN) delay_cycles(CS_GAP_US * (F_CPU / 1000000UL));   /* trap 1 */
  }
}

PinStatus digitalRead(pin_size_t pin)
{
  if (pin >= NUM_DIGITAL_PINS) compat_bad_pin(pin);
  return DL_GPIO_readPins(pin_port(pin), pin_mask(pin)) ? HIGH : LOW;
}

extern "C" volatile uint32_t *compat_port_out(uint8_t port) { return &(port ? GPIOB : GPIOA)->DOUT31_0; }
extern "C" volatile uint32_t *compat_port_in(uint8_t port)  { return &(port ? GPIOB : GPIOA)->DIN31_0; }

/* ---------- random (the usual core versions on top of the C library) ---------- */
long random(long howbig) { return howbig == 0 ? 0 : rand() % howbig; }
long random(long howsmall, long howbig)
{
  if (howsmall >= howbig) return howsmall;
  return random(howbig - howsmall) + howsmall;
}
void randomSeed(unsigned long seed) { if (seed != 0) srand(seed); }

/* ---------- start-up ---------- */
extern "C" void initVariant(void) __attribute__((weak));
extern "C" void initVariant(void) {}

void init(void)
{
  SYSCFG_DL_init();                       /* clock, SPI1, UART0 from syscfg/arduino.syscfg */
  DL_GPIO_enablePower(GPIOA);             /* already on; kept so pinMode works even if SysConfig changes */
  DL_GPIO_enablePower(GPIOB);
  SysTick->LOAD = SYSTICK_LOAD;
  SysTick->VAL = 0;
  SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}
