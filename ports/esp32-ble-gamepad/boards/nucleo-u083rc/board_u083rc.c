/* Thin board glue for NUCLEO-U083RC: HSI16 clock, USART2 (ST-LINK VCP, PA2/PA3) at 115200,
   SysTick 1 ms uptime, TIM2 as a 1 MHz free-running timer, start-up code. Calls frl_main(). */
#include "stm32u083xx.h"
#include "board.h"

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack, _ram_end;
extern int main(void);

static volatile uint32_t ms_ticks;
void SysTick_Handler(void) { ms_ticks++; }

void Reset_Handler(void)
{
  uint32_t *s = &_sidata, *d = &_sdata;
  while (d < &_edata) *d++ = *s++;
  for (d = &_sbss; d < &_ebss; ) *d++ = 0;
  main();
  for (;;) {}
}
void Default_Handler(void) { for (;;) {} }

__attribute__((section(".isr_vector"), used))
const void *const vectors[48] = {
  &_estack, Reset_Handler, Default_Handler, Default_Handler, 0, 0, 0, 0, 0, 0, 0, Default_Handler,
  0, 0, Default_Handler, SysTick_Handler,
};

void board_init(void)
{
  RCC->CR |= RCC_CR_HSION;
  while (!(RCC->CR & RCC_CR_HSIRDY)) {}
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | 1u;              /* SW = HSI16 */
  while (((RCC->CFGR & RCC_CFGR_SWS) >> RCC_CFGR_SWS_Pos) != 1u) {}

  RCC->IOPENR |= RCC_IOPENR_GPIOAEN;
  RCC->APBENR1 |= RCC_APBENR1_USART2EN | RCC_APBENR1_TIM2EN;
  /* PA2 = USART2_TX AF7, PA3 = USART2_RX AF7 */
  GPIOA->MODER = (GPIOA->MODER & ~((3u << 4) | (3u << 6))) | (2u << 4) | (2u << 6);
  GPIOA->AFR[0] = (GPIOA->AFR[0] & ~((0xFu << 8) | (0xFu << 12))) | (7u << 8) | (7u << 12);
  USART2->BRR = 16000000u / 115200u;
  USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

  TIM2->PSC = 15;            /* 16 MHz / 16 = 1 MHz */
  TIM2->EGR = TIM_EGR_UG;    /* load the prescaler now */
  TIM2->CR1 = TIM_CR1_CEN;

  SysTick->LOAD = 16000u - 1u;
  SysTick->VAL = 0;
  SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}
void board_putc(char c)
{
  while (!(USART2->ISR & USART_ISR_TXE_TXFNF)) {}
  USART2->TDR = (uint8_t)c;
}
uint32_t board_us(void) { return TIM2->CNT; }
uint32_t board_ms(void) { return ms_ticks; }
uint32_t board_free_ram(void)
{
  return (uint32_t)((char *)&_ram_end - (char *)&_ebss);
}

/* no libc: the two routines the core uses */
void *memcpy(void *d, const void *s, unsigned n) { uint8_t *a = d; const uint8_t *b = s; while (n--) *a++ = *b++; return d; }
void *memset(void *d, int v, unsigned n) { uint8_t *a = d; while (n--) *a++ = (uint8_t)v; return d; }
