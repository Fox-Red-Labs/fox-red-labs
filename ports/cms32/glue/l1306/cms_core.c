/*
 * Cmsemicon CMS32F04xx driver names, implemented on the TI MSPM0L1306 with DriverLib.
 * GPIO, SysTick and delay, interrupt registration, reset cause, clock switching,
 * flash, watchdog refresh, interval timer, sleep, ADC and printf.
 */
#include <stdarg.h>
#include <string.h>
#include "CMS32F04xx.h"
#include "cms_pinmap.h"
#include "uart_demo.h"

uint32_t SystemCoreClock = 32000000u;
void SystemCoreClockUpdate(void) { /* SYSOSC: 32 MHz unless CGC_HSI_Select changed it */ }

CMS_RST_t cms_rst;
CMS_CGC_t cms_cgc;
CMS_TSN_t cms_tsn;

/* ---- start-up: power the blocks the examples expect to find on ------------ */
void cms_core_init(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_enablePower(GPIOA);
    delay_cycles(16);
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);
    DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);

    /* reset cause, in Cmsemicon's RESF bit positions */
    uint32_t c = (uint32_t) DL_SYSCTL_getResetCause();
    uint32_t r = 0;
    if (c == DL_SYSCTL_RESET_CAUSE_SYSRST_WWDT0_VIOLATION ||
        c == DL_SYSCTL_RESET_CAUSE_SYSRST_WWDT1_VIOLATION)
        r |= RST_RESF_WWDGRF_Msk | RST_RESF_IWDGRF_Msk;   /* one watchdog plays both */
    if (c == DL_SYSCTL_RESET_CAUSE_SYSRST_SW_TRIGGERED || c == DL_SYSCTL_RESET_CAUSE_CPURST_SW_TRIGGERED ||
        c == DL_SYSCTL_RESET_CAUSE_POR_SW_TRIGGERED || c == DL_SYSCTL_RESET_CAUSE_BOOTRST_SW_TRIGGERED)
        r |= RST_RESF_SWRF_Msk;
    if (c == DL_SYSCTL_RESET_CAUSE_POR_EXTERNAL_NRST || c == DL_SYSCTL_RESET_CAUSE_BOOTRST_EXTERNAL_NRST)
        r |= RST_RESF_NRSTRF_Msk;
    if (c == DL_SYSCTL_RESET_CAUSE_BOR_SUPPLY_FAILURE) r |= RST_RESF_LVDRF_Msk;
    if (c == DL_SYSCTL_RESET_CAUSE_POR_HW_FAILURE) r |= RST_RESF_PORRF_Msk;
    cms_rst.RESF = r;
    cms_cgc.CKC  = 0;

    /* temperature sensor: Cmsemicon stores the ADC code at 25 C and 85 C. The MSPM0L1306
     * stores one point (TRIM at 30 C for a 1.4 V reference); its data sheet gives the slope.
     * These two numbers reproduce the data sheet model for a 3.3 V ADC reference so the
     * example's formula runs; they are not a calibration. */
    cms_tsn.TSN25 = 1540;
    cms_tsn.TSN85 = 1430;
}

/* ---- SysTick and delays ---------------------------------------------------- */
__attribute__((weak)) volatile uint32_t g_ticks = 0xFFFFFFFFu;   /* some main.c files define their own */
__attribute__((weak)) void SysTick_Handler(void) { if (g_ticks != 0) g_ticks--; }

static uint32_t cycles_per_us = 32;
void delay_init(uint32_t sysclk) { cycles_per_us = sysclk / 1000000u; if (cycles_per_us == 0) cycles_per_us = 1; }
void m0_delay_us(uint32_t us) { while (us--) delay_cycles(cycles_per_us); }
void m0_delay_ms(uint32_t ms) { while (ms--) m0_delay_us(1000); }

/* ---- interrupt registration ----------------------------------------------- */
static cms_isr_t cms_isr_table[32];
void ISR_Register(CMS_IRQn_t irq, cms_isr_t fn)
{
    if ((unsigned) irq < 32) cms_isr_table[irq] = fn;
    switch (irq) {
    case SCI_IRQn:     NVIC_EnableIRQ(UART0_INT_IRQn); break;
    case USART1_IRQn:  NVIC_EnableIRQ(UART0_INT_IRQn); break;   /* USART1 is UART0 here */
    case ADC_IRQn:     NVIC_EnableIRQ(ADC0_INT_IRQn); break;
    case TIM1BRK_IRQn:
    case TIM1CC_IRQn:  NVIC_EnableIRQ(TIMG0_INT_IRQn); break;
    case LPTIM_IRQn:   NVIC_EnableIRQ(TIMG1_INT_IRQn); break;
    case TIM3_IRQn:
    case TIM17_IRQn:   NVIC_EnableIRQ(TIMG2_INT_IRQn); break;
    case TIM14_IRQn:
    case TIM16_IRQn:   NVIC_EnableIRQ(TIMG4_INT_IRQn); break;
    case INTP01_IRQn:
    case INTP23_IRQn:
    case INTP4To15_IRQn:
    case ACMP0_IRQn:
    case ACMP1_IRQn:   NVIC_EnableIRQ(GPIOA_INT_IRQn); break;   /* GROUP1: GPIOA and COMP0 */
    case WWDG_IRQn:    NVIC_EnableIRQ(WWDT0_INT_IRQn); break;   /* GROUP0 */
    case DMA0_IRQn:
    case DMA12_IRQn:
    case DMA3to7_IRQn: NVIC_EnableIRQ(DMA_INT_IRQn); break;
    case I2C_IRQn:     NVIC_EnableIRQ(I2C0_INT_IRQn); break;
    case SPI_IRQn:     NVIC_EnableIRQ(SPI0_INT_IRQn); break;
    default: break;
    }
}
int cms_isr_registered(CMS_IRQn_t irq) { return (unsigned) irq < 32 && cms_isr_table[irq] != 0; }
void cms_isr_dispatch(CMS_IRQn_t irq) { if ((unsigned) irq < 32 && cms_isr_table[irq]) cms_isr_table[irq](); }

/* TI handlers: each one hands the event to the Cmsemicon-numbered handler the example registered */
void TIMG0_IRQHandler(void)
{
    switch (DL_TimerG_getPendingInterrupt(TIMG0)) {
    case DL_TIMERG_IIDX_ZERO:
    case DL_TIMERG_IIDX_LOAD: cms_isr_dispatch(TIM1BRK_IRQn); break;   /* "update" */
    default:                  cms_isr_dispatch(TIM1CC_IRQn); break;
    }
}
void TIMG1_IRQHandler(void) { (void) DL_TimerG_getPendingInterrupt(TIMG1); cms_isr_dispatch(LPTIM_IRQn); }
void TIMG2_IRQHandler(void)
{
    (void) DL_TimerG_getPendingInterrupt(TIMG2);
    if (cms_isr_table[TIM3_IRQn]) cms_isr_dispatch(TIM3_IRQn); else cms_isr_dispatch(TIM17_IRQn);
}
void TIMG4_IRQHandler(void)
{
    (void) DL_TimerG_getPendingInterrupt(TIMG4);
    if (cms_isr_table[TIM16_IRQn]) cms_isr_dispatch(TIM16_IRQn); else cms_isr_dispatch(TIM14_IRQn);
}
void ADC0_IRQHandler(void) { (void) DL_ADC12_getPendingInterrupt(ADC0); cms_isr_dispatch(ADC_IRQn); }
void GROUP1_IRQHandler(void)
{
    switch (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1)) {
    case DL_INTERRUPT_GROUP1_IIDX_GPIOA:
        if (cms_isr_table[INTP01_IRQn]) cms_isr_dispatch(INTP01_IRQn); else cms_isr_dispatch(INTP4To15_IRQn);
        break;
    case DL_INTERRUPT_GROUP1_IIDX_COMP0:
        (void) DL_COMP_getPendingInterrupt(COMP0);
        if (cms_isr_table[ACMP1_IRQn]) cms_isr_dispatch(ACMP1_IRQn); else cms_isr_dispatch(ACMP0_IRQn);
        break;
    default: break;
    }
}
void GROUP0_IRQHandler(void)
{
    if (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_0) == DL_INTERRUPT_GROUP0_IIDX_WWDT0) {
        (void) DL_WWDT_getPendingInterrupt(WWDT0);
        cms_isr_dispatch(WWDG_IRQn);
    }
}
void DMA_IRQHandler(void)
{
    switch (DL_DMA_getPendingInterrupt(DMA)) {
    case DL_DMA_EVENT_IIDX_DMACH0: cms_isr_dispatch(DMA0_IRQn); break;
    case DL_DMA_EVENT_IIDX_DMACH1:
    case DL_DMA_EVENT_IIDX_DMACH2: cms_isr_dispatch(DMA12_IRQn); break;
    default: cms_isr_dispatch(DMA3to7_IRQn); break;
    }
}
void I2C0_IRQHandler(void) { cms_isr_dispatch(I2C_IRQn); }
void SPI0_IRQHandler(void) { cms_isr_dispatch(SPI_IRQn); }

/* ---- GPIO ------------------------------------------------------------------ */
uint32_t cms_pin_mask(PORT_TypeDef port, PIN_TypeDef pin) { return 1u << cms_pin_index(port, pin); }
uint32_t cms_pin_pincm(PORT_TypeDef port, PIN_TypeDef pin) { return cms_pin_index(port, pin) + 1; } /* PAn is PINCM(n+1) */

void PORT_Init(PORT_TypeDef port, PIN_TypeDef pin, PIN_ModeDef mode)
{
    uint32_t m = cms_pin_mask(port, pin), cm = cms_pin_pincm(port, pin);
    switch (mode) {
    case OUTPUT:
        DL_GPIO_initDigitalOutput(cm);
        DL_GPIO_enableOutput(GPIOA, m);
        break;
    case OPENDRAIN_OUTPUT:
        DL_GPIO_initDigitalOutputFeatures(cm, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
                                          DL_GPIO_DRIVE_STRENGTH_LOW, DL_GPIO_HIZ_ENABLE);
        DL_GPIO_enableOutput(GPIOA, m);
        break;
    case PULLUP_INPUT:
        DL_GPIO_initDigitalInputFeatures(cm, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
                                         DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
        break;
    case ANALOG_INPUT:
        DL_GPIO_initPeripheralAnalogFunction(cm);
        break;
    case INPUT:
    case TTL_INPUT:
    default:
        DL_GPIO_initDigitalInput(cm);
        break;
    }
}
void PORT_SetBit(PORT_TypeDef port, PIN_TypeDef pin)    { DL_GPIO_setPins(GPIOA, cms_pin_mask(port, pin)); }
void PORT_ClrBit(PORT_TypeDef port, PIN_TypeDef pin)    { DL_GPIO_clearPins(GPIOA, cms_pin_mask(port, pin)); }
void PORT_ToggleBit(PORT_TypeDef port, PIN_TypeDef pin) { DL_GPIO_togglePins(GPIOA, cms_pin_mask(port, pin)); }
uint8_t PORT_GetBit(PORT_TypeDef port, PIN_TypeDef pin) { return (DL_GPIO_readPins(GPIOA, cms_pin_mask(port, pin)) != 0); }
void GPIO_Output_Enable(PORT_TypeDef port, PIN_TypeDef pin) { PORT_Init(port, pin, OUTPUT); }
void GPIO_Input_Enable(PORT_TypeDef port, PIN_TypeDef pin)  { PORT_Init(port, pin, INPUT); }
void GPIO_PullUp_Enable(PORT_TypeDef port, PIN_TypeDef pin) { PORT_Init(port, pin, PULLUP_INPUT); }
void RESTPinGpio_Set(uint8_t state) { (void) state; }

/* ---- clocks ------------------------------------------------------------------ */
void CGC_SubOsc_Setting(OSC_Mode_t mode, OSC_Drv_t drv) { (void) mode; (void) drv; /* LFOSC is always on */ }
void CGC_MainOsc_Setting(OSC_Mode_t mode, OSC_Drv_t drv) { (void) mode; (void) drv; /* no HFXT on this board */ }
void CGC_Fclk_Select(MainClk_t sel)
{
    switch (sel) {
    case MAINCLK_FSUB:   /* run the CPU from the 32 kHz LFCLK, as the example asks */
        DL_SYSCTL_switchMCLKfromSYSOSCtoLFCLK(false);
        SystemCoreClock = 32768u;
        cms_cgc.CKC |= CGC_CKC_CLS_Msk;
        break;
    case MAINCLK_FIH:
        DL_SYSCTL_switchMCLKfromLFCLKtoSYSOSC();
        SystemCoreClock = 32000000u;
        cms_cgc.CKC &= ~CGC_CKC_CLS_Msk;
        break;
    default:             /* FMX, PLL: the MSPM0L1306 has neither; stay on SYSOSC */
        break;
    }
    delay_init(SystemCoreClock);
}
void CGC_HSI_Select(HSI_Freq_t f)
{
    /* SYSOSC offers 32 MHz or 4 MHz; the example's 6 MHz becomes 4 MHz */
    if (f == HSI_FREQ_48MHZ || f == HSI_FREQ_24MHZ || f == HSI_FREQ_12MHZ) {
        DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE); SystemCoreClock = 32000000u;
    } else {
        DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_4M); SystemCoreClock = 4000000u;
    }
    delay_init(SystemCoreClock);
}

/* ---- flash ------------------------------------------------------------------- */
/* The CMS32F040 has 128 kB; the examples write at 0x1F000 and up. The MSPM0L1306 has
 * 64 kB, so addresses above 64 kB are folded down by 64 kB (0x1F000 -> 0xF000). */
static uint32_t flash_adr(uint32_t adr) { return (adr >= 0x10000u && adr < 0x20000u) ? adr - 0x10000u : adr; }
int EraseSector(uint32_t adr)
{
    adr = flash_adr(adr) & ~0x3FFu;      /* MSPM0 sectors are 1 kB */
    DL_FlashCTL_executeClearStatus(FLASHCTL);
    DL_FlashCTL_unprotectSector(FLASHCTL, adr, DL_FLASHCTL_REGION_SELECT_MAIN);
    DL_FlashCTL_eraseMemoryFromRAM(FLASHCTL, adr, DL_FLASHCTL_COMMAND_SIZE_SECTOR);
    return DL_FlashCTL_waitForCmdDone(FLASHCTL) ? 0 : 1;
}
/* MSPM0 flash programs 64-bit words; a second write into the same 8 bytes is not allowed. So the glue
 * writes whole 8-byte words, padding with 0xFF, from an 8-byte-aligned address. */
int ProgramPage(uint32_t adr, uint32_t sz, uint8_t *buf)
{
    adr = flash_adr(adr);
    uint32_t off = adr & 7u; adr -= off;
    for (uint32_t i = 0; i < off + sz; i += 8) {
        uint8_t w[8]; memset(w, 0xFF, 8);
        for (uint32_t k = 0; k < 8; k++) { int32_t src = (int32_t) (i + k) - (int32_t) off; if (src >= 0 && (uint32_t) src < sz) w[k] = buf[src]; }
        DL_FlashCTL_executeClearStatus(FLASHCTL);
        DL_FlashCTL_unprotectSector(FLASHCTL, adr + i, DL_FLASHCTL_REGION_SELECT_MAIN);
        DL_FlashCTL_programMemoryFromRAM64(FLASHCTL, adr + i, (uint32_t *) w);
        if (!DL_FlashCTL_waitForCmdDone(FLASHCTL)) return 1;
    }
    return 0;
}
void FLASH_Read(uint8_t *read_address, uint16_t size, uint8_t *readbuf)
{
    memcpy(readbuf, (const void *) flash_adr((uint32_t) read_address), size);
}
int FLASH_Code_Write(uint32_t adr, uint32_t sz, uint8_t *buf) { return ProgramPage(adr, sz * 4u, buf); }
void FLASH_Write_Byte(uint8_t area, uint32_t adr, uint8_t dat)
{
    /* no separate data flash on the MSPM0L1306: the data region lands in the last 1 kB sector */
    (void) area;
    uint32_t a = 0xFC00u + (adr & 0x3F8u);
    ProgramPage(a, 1, &dat);
}

/* ---- watchdog refresh and interval timer (configured in iwdg_demo.c, wwdg_demo.c, interval_demo.c) */
void WWDG_SetCounter(uint8_t counter) { (void) counter; DL_WWDT_restart(WWDT0); }
void IT_Start(void) { DL_TimerG_startCounter(TIMG1); }
void IT_Stop(void)  { DL_TimerG_stopCounter(TIMG1); }

/* ---- SPI names used by one main.c ------------------------------------------ */
void SPI_ITConfig(uint8_t it, FunctionalState state) { (void) it; (void) state; }
void SPI_Cmd(FunctionalState state) { if (state) DL_SPI_enable(SPI0); else DL_SPI_disable(SPI0); }

/* ---- printf to UART0 --------------------------------------------------------- */
int printf(const char *fmt, ...)
{
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    for (int i = 0; i < n && i < (int) sizeof buf - 1; i++) {
        if (buf[i] == '\n') Uart0_Send('\r');
        Uart0_Send((uint8_t) buf[i]);
    }
    return n;
}

/* TI's C runtime calls this after .data and .bss are set up and before main(): the one hook that lets
 * the examples' main.c stay as shipped while the chip gets its power-on setup. */
void _system_post_cinit(void) { cms_core_init(); }
