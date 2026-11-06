/*
 * Stand-in for Cmsemicon's CMS32F04xx.h on the TI MSPM0L1306.
 *
 * The Cmsemicon examples' main.c files include this header and call a small set of
 * names from it. This file gives those names a meaning on the MSPM0L1306: the same
 * function names and enums, implemented in cms_core.c on TI DriverLib. Nothing here
 * copies Cmsemicon code; it is the vendor glue layer of docs/../portability/driver-layering.md.
 *
 * Pin mapping (Cmsemicon port/pin to LP-MSPM0L1306 pin) lives in cms_pinmap.h.
 */
#ifndef CMS32F04XX_COMPAT_H
#define CMS32F04XX_COMPAT_H

#include <stdint.h>
#include <stdio.h>
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- core / clock -------------------------------------------------------- */
extern uint32_t SystemCoreClock;        /* 32 MHz SYSOSC after reset */
void SystemCoreClockUpdate(void);
void cms_core_init(void);               /* called before main() from the startup hook */

typedef enum { RESET = 0, SET = !RESET } FlagStatus, ITStatus;
typedef enum { ERROR = 0, SUCCESS = !ERROR } ErrorStatus;
typedef enum { DISABLE = 0, ENABLE = !DISABLE } FunctionalState;

/* Cmsemicon interrupt numbers, used only as keys for ISR_Register() */
typedef enum {
    WWDG_IRQn = 0, INTLVI_IRQn = 1, CAN0ERR_IRQn = 2, FMC_IRQn = 3, CAN0TxRx_IRQn = 4,
    INTP01_IRQn = 5, INTP23_IRQn = 6, INTP4To15_IRQn = 7, CAN0WUP_IRQn = 8, DMA0_IRQn = 9,
    DMA12_IRQn = 10, DMA3to7_IRQn = 11, ADC_IRQn = 12, TIM1BRK_IRQn = 13, TIM1CC_IRQn = 14,
    USART4_IRQn = 15, TIM3_IRQn = 16, LPTIM_IRQn = 17, PD_IRQn = 18, TIM14_IRQn = 19,
    LCD_IRQn = 20, TIM16_IRQn = 21, TIM17_IRQn = 22, I2C_IRQn = 23, OSDC_IRQn = 24,
    SPI_IRQn = 25, SCI_IRQn = 26, USART1_IRQn = 27, USART2_IRQn = 28, USART3_IRQn = 29,
    ACMP0_IRQn = 30, ACMP1_IRQn = 31
} CMS_IRQn_t;
typedef void (*cms_isr_t)(void);
void ISR_Register(CMS_IRQn_t irq, cms_isr_t fn);
void cms_isr_dispatch(CMS_IRQn_t irq);  /* calls the registered handler, if any */
int  cms_isr_registered(CMS_IRQn_t irq);

/* ---- GPIO (Cmsemicon names) ---------------------------------------------- */
typedef enum { PORTA = 0, PORTB = 1, PORTC = 2, PORTF = 5 } PORT_TypeDef;
typedef enum { PIN0 = 0, PIN1, PIN2, PIN3, PIN4, PIN5, PIN6, PIN7, PIN8, PIN9, PIN10, PIN11,
               PIN12, PIN13, PIN14, PIN15 } PIN_TypeDef;
typedef enum { INPUT = 0, OUTPUT = 1, PULLUP_INPUT = 2, ANALOG_INPUT = 3, OPENDRAIN_OUTPUT = 4,
               TTL_INPUT = 5 } PIN_ModeDef;

void PORT_Init(PORT_TypeDef port, PIN_TypeDef pin, PIN_ModeDef mode);
void PORT_SetBit(PORT_TypeDef port, PIN_TypeDef pin);
void PORT_ClrBit(PORT_TypeDef port, PIN_TypeDef pin);
void PORT_ToggleBit(PORT_TypeDef port, PIN_TypeDef pin);
uint8_t PORT_GetBit(PORT_TypeDef port, PIN_TypeDef pin);
void GPIO_Output_Enable(PORT_TypeDef port, PIN_TypeDef pin);
void GPIO_Input_Enable(PORT_TypeDef port, PIN_TypeDef pin);
void GPIO_PullUp_Enable(PORT_TypeDef port, PIN_TypeDef pin);
void RESTPinGpio_Set(uint8_t state);    /* no-op: NRST is a dedicated pin on the MSPM0 */
uint32_t cms_pin_mask(PORT_TypeDef port, PIN_TypeDef pin);   /* DL_GPIO_PIN_x for GPIOA */
uint32_t cms_pin_pincm(PORT_TypeDef port, PIN_TypeDef pin);  /* IOMUX_PINCMx */

/* ---- reset cause (read-only shadow filled at start-up) ------------------- */
typedef struct { volatile uint32_t RESF; } CMS_RST_t;
extern CMS_RST_t cms_rst;
#define RST (&cms_rst)
#define RST_RESF_LVDRF_Msk   (0x01UL)
#define RST_RESF_SWRF_Msk    (0x04UL)
#define RST_RESF_WWDGRF_Msk  (0x10UL)
#define RST_RESF_IWDGRF_Msk  (0x20UL)
#define RST_RESF_PORRF_Msk   (0x40UL)
#define RST_RESF_NRSTRF_Msk  (0x80UL)

/* ---- clock generator (Cmsemicon CGC names) ------------------------------- */
typedef struct { volatile uint32_t CKC; } CMS_CGC_t;   /* status shadow kept by the glue */
extern CMS_CGC_t cms_cgc;
#define CGC (&cms_cgc)
#define CGC_CKC_CLS_Msk (0x10UL)   /* main clock is the sub (32 kHz) clock */
#define CGC_CKC_MCS_Msk (0x40UL)   /* main clock is the external main oscillator */
typedef enum { OSC_PORT = 0, OSC_OSCILLATOR = 1, OSC_EXCLK = 2 } OSC_Mode_t;
typedef enum { OSC_DRV_LOW = 0, OSC_DRV_HIGH = 1 } OSC_Drv_t;
typedef enum { MAINCLK_FIH = 0, MAINCLK_FMX = 1, MAINCLK_FSUB = 2, MAINCLK_PLL = 3 } MainClk_t;
typedef enum { HSI_FREQ_48MHZ = 0, HSI_FREQ_24MHZ, HSI_FREQ_12MHZ, HSI_FREQ_6MHZ, HSI_FREQ_3MHZ,
               HSI_FREQ_2MHZ, HSI_FREQ_1MHZ } HSI_Freq_t;
void CGC_SubOsc_Setting(OSC_Mode_t mode, OSC_Drv_t drv);
void CGC_MainOsc_Setting(OSC_Mode_t mode, OSC_Drv_t drv);
void CGC_Fclk_Select(MainClk_t sel);
void CGC_HSI_Select(HSI_Freq_t f);

/* ---- temperature-sensor trim shadow (see cms_core.c) --------------------- */
typedef struct { int16_t TSN25; int16_t TSN85; } CMS_TSN_t;
extern CMS_TSN_t cms_tsn;
#define TSN (&cms_tsn)

/* ---- ADC names used by main.c -------------------------------------------- */
typedef struct { volatile uint32_t ADCR; } CMS_ADC_t;
/* &ADC->ADCR is a real address: ADC0 conversion memory 0, so DMA can read it */
#define ADC ((CMS_ADC_t *)&ADC0->ULLMEM.MEMRES[0])
typedef enum { ADC_Channel_0 = 0, ADC_Channel_1, ADC_Channel_2, ADC_Channel_3, ADC_Channel_4,
               ADC_Channel_5, ADC_Channel_6, ADC_Channel_7, ADC_Channel_8, ADC_Channel_9,
               ADC_Channel_Internal = 11, ADC_Channel_Off = 0x3f } ADC_Channel_t;
uint16_t ADC_Converse(ADC_Channel_t ch, uint32_t n, uint16_t *buf);
void ADC_Start(ADC_Channel_t ch);
void ADC_DMA_Init(ADC_Channel_t ch);
double adc_get_temperature(int16_t dat, double CalcVolt);

/* ---- DMA names used by main.c -------------------------------------------- */
typedef enum { DMA_VECTOR_ADC = 0, DMA_VECTOR_USART1_TX = 1, DMA_VECTOR_USART1_RX = 2,
               DMA_VECTOR_SPI_TX = 3, DMA_VECTOR_SPI_RX = 4, DMA_VECTOR_TIM1_CC = 5 } DMA_VECTOR_t;
typedef enum { DMA_Mode_Normal = 0, DMA_Mode_Repeat = 1 } DMA_Mode_t;
#define ADCDMA_Channel 0

/* ---- interval timer, watchdog, flash, sleep ------------------------------ */
#define IT_FSUB 0x00
#define IT_FIL  0x01
void IT_Start(void);
void IT_Stop(void);
void WWDG_SetCounter(uint8_t counter);

#define FLASH_REGION_Code 0x55
#define FLASH_REGION_Data 0xAA
int EraseSector(uint32_t adr);
int ProgramPage(uint32_t adr, uint32_t sz, uint8_t *buf);
void FLASH_Read(uint8_t *read_address, uint16_t size, uint8_t *readbuf);
int FLASH_Code_Write(uint32_t adr, uint32_t sz, uint8_t *buf);
void FLASH_Write_Byte(uint8_t area, uint32_t adr, uint8_t dat);

#define __SLEEP()     do { DL_SYSCTL_setPowerPolicyRUN0SLEEP0(); __WFI(); } while (0)
#define __DEEPSLEEP() do { DL_SYSCTL_setPowerPolicySTOP0(); __WFI(); } while (0)

/* ---- SPI names used by main.c (spiMasterSendReceive) --------------------- */
#define SPI_IT_TXE  0x01
#define SPI_IT_RXNE 0x02
void SPI_ITConfig(uint8_t it, FunctionalState state);
void SPI_Cmd(FunctionalState state);
#define BUFFERSIZE 32

/* printf goes to UART0 (the LaunchPad kit's USB serial port); see cms_core.c */
int printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif
#endif
