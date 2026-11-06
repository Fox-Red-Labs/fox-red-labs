/*
 * Timers. Cmsemicon TIM1 (advanced, 4 channels, complementary outputs) -> TIMG0 (2 channels, no
 * complementary outputs on the MSPM0L1306); TIM3 and TIM17 -> TIMG2; TIM14 and TIM16 -> TIMG4.
 * TIMG1 is kept for the interval timer. What each variant shows is noted inline.
 */
#include "tim_demo.h"
#include "gpio_demo.h"

static const DL_TimerG_ClockConfig clk_bus1 = { .clockSel = DL_TIMER_CLOCK_BUSCLK, .divideRatio = DL_TIMER_CLOCK_DIVIDE_1, .prescale = 0 };

static void timg_power(GPTIMER_Regs *t) { DL_TimerG_reset(t); DL_TimerG_enablePower(t); delay_cycles(16); }

/* --- TIM1 as a 17.57 kHz PWM on 2 pins (the example drives 4 at 50, 37.5, 25 and 12.5 percent) --- */
static void tim1_pwm(void)
{
    timg_power(TIMG0);
    DL_GPIO_initPeripheralOutputFunction(IOMUX_PINCM13, IOMUX_PINCM13_PF_TIMG0_CCP0);   /* PA12 */
    DL_GPIO_initPeripheralOutputFunction(IOMUX_PINCM14, IOMUX_PINCM14_PF_TIMG0_CCP1);   /* PA13, RGB red */
    DL_GPIO_enableOutput(GPIOA, DL_GPIO_PIN_12 | DL_GPIO_PIN_13);
    uint32_t period = SystemCoreClock / 17570u - 1;
    DL_TimerG_PWMConfig pwm = { .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN, .period = period, .isTimerWithFourCC = false, .startTimer = DL_TIMER_STOP };
    DL_TimerG_setClockConfig(TIMG0, (DL_TimerG_ClockConfig *) &clk_bus1);
    DL_TimerG_initPWMMode(TIMG0, &pwm);
    DL_TimerG_setCaptureCompareValue(TIMG0, period * 5 / 10, DL_TIMER_CC_0_INDEX);      /* 50 % */
    DL_TimerG_setCaptureCompareValue(TIMG0, period * 375 / 1000, DL_TIMER_CC_1_INDEX);  /* 37.5 % */
    DL_TimerG_setCCPDirection(TIMG0, DL_TIMER_CC0_OUTPUT | DL_TIMER_CC1_OUTPUT);
    DL_TimerG_enableClock(TIMG0);
    DL_TimerG_startCounter(TIMG0);
}
/* --- TIM1 as a periodic interrupt (STB/sleep): 17.57 kHz update rate in the original --- */
static void tim1_periodic(void)
{
    timg_power(TIMG0);
    DL_TimerG_TimerConfig t = { .period = SystemCoreClock / 17570u - 1, .timerMode = DL_TIMER_TIMER_MODE_PERIODIC, .startTimer = DL_TIMER_STOP };
    DL_TimerG_setClockConfig(TIMG0, (DL_TimerG_ClockConfig *) &clk_bus1);
    DL_TimerG_initTimerMode(TIMG0, &t);
    DL_TimerG_enableInterrupt(TIMG0, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    DL_TimerG_enableClock(TIMG0);
    ISR_Register(TIM1BRK_IRQn, timer1_Update_interrupt);
    DL_TimerG_startCounter(TIMG0);
}
void TIM1_Config(void)
{
#if defined(CMS_EXAMPLE_STB_sleep)
    tim1_periodic();
#else
    tim1_pwm();
#endif
}
void timer1_CC_interrupt(void) { }
void timer1_Update_interrupt(void)
{
    LED1_Toggle;
}

/* --- TIM1 TRGO: the timer's zero event is published so the ADC can be triggered by it (adc_demo.c subscribes) --- */
void TIM1_Config_TRGO(void)
{
    timg_power(TIMG0);
    DL_TimerG_TimerConfig t = { .period = 31999, .timerMode = DL_TIMER_TIMER_MODE_PERIODIC, .startTimer = DL_TIMER_STOP };  /* 1 kHz */
    DL_TimerG_setClockConfig(TIMG0, (DL_TimerG_ClockConfig *) &clk_bus1);
    DL_TimerG_initTimerMode(TIMG0, &t);
    DL_TimerG_enableClock(TIMG0);
    DL_TimerG_enableEvent(TIMG0, DL_TIMERG_EVENT_ROUTE_1, DL_TIMERG_EVENT_ZERO_EVENT);
    DL_TimerG_setPublisherChanID(TIMG0, DL_TIMERG_PUBLISHER_INDEX_0, 1);
    DL_TimerG_startCounter(TIMG0);
}

/* --- TIM1 duty updated by DMA (timer1_dma): a table of 8 duty values fed to CC0 on every period --- */
static const uint32_t duty_table[8] = {100, 300, 500, 700, 900, 1100, 1300, 1500};
void TIM1_DMA_Config(void)
{
    tim1_pwm();
    DL_TimerG_setLoadValue(TIMG0, 1600);
    DL_DMA_Config d = { .transferMode = DL_DMA_SINGLE_TRANSFER_MODE, .extendedMode = DL_DMA_NORMAL_MODE,
        .destIncrement = DL_DMA_ADDR_UNCHANGED, .srcIncrement = DL_DMA_ADDR_INCREMENT,
        .destWidth = DL_DMA_WIDTH_WORD, .srcWidth = DL_DMA_WIDTH_WORD,
        .trigger = DMA_GENERIC_SUB0_TRIG, .triggerType = DL_DMA_TRIGGER_TYPE_EXTERNAL };
    DL_DMA_initChannel(DMA, 0, &d);
    DL_DMA_setSrcAddr(DMA, 0, (uint32_t) duty_table);
    DL_DMA_setDestAddr(DMA, 0, (uint32_t) &TIMG0->COUNTERREGS.CC_01[0]);
    DL_DMA_setTransferSize(DMA, 0, 8);
    DL_DMA_enableChannel(DMA, 0);
}

/* --- TIM3: compare interrupts (the original schedules 4 channels; TIMG2 has 2) --- */
static volatile uint32_t capture;
void TIM3_Config(void)
{
    timg_power(TIMG2);
    DL_TimerG_ClockConfig c = { .clockSel = DL_TIMER_CLOCK_BUSCLK, .divideRatio = DL_TIMER_CLOCK_DIVIDE_1, .prescale = (SystemCoreClock / 2 / 6000000u) - 1 };
    DL_TimerG_TimerConfig t = { .period = 65535, .timerMode = DL_TIMER_TIMER_MODE_PERIODIC_UP, .startTimer = DL_TIMER_STOP };
    DL_TimerG_setClockConfig(TIMG2, &c);
    DL_TimerG_initTimerMode(TIMG2, &t);
    DL_TimerG_setCaptureCompareValue(TIMG2, 40961, DL_TIMER_CC_0_INDEX);
    DL_TimerG_setCaptureCompareValue(TIMG2, 27309, DL_TIMER_CC_1_INDEX);
    DL_TimerG_enableInterrupt(TIMG2, DL_TIMERG_INTERRUPT_CC0_UP_EVENT | DL_TIMERG_INTERRUPT_CC1_UP_EVENT);
    DL_TimerG_enableClock(TIMG2);
    ISR_Register(TIM3_IRQn, timer3_interrupt);
    DL_TimerG_startCounter(TIMG2);
}
void timer3_interrupt(void)
{
    capture = DL_TimerG_getCaptureCompareValue(TIMG2, DL_TIMER_CC_0_INDEX);
    DL_TimerG_setCaptureCompareValue(TIMG2, (capture + 40961) & 0xFFFF, DL_TIMER_CC_0_INDEX);
    LED1_Toggle;
}

/* --- TIM14 (three gated timers in a chain in the original: not reproducible on TIMG) and the 5 ms periodic variant --- */
void TIM14_Config(void) { TIM14_timing_Config(); }
void TIM14_timing_Config(void)
{
    timg_power(TIMG4);
    DL_TimerG_ClockConfig c = { .clockSel = DL_TIMER_CLOCK_BUSCLK, .divideRatio = DL_TIMER_CLOCK_DIVIDE_1, .prescale = 31 };  /* 1 MHz */
    DL_TimerG_TimerConfig t = { .period = 4999, .timerMode = DL_TIMER_TIMER_MODE_PERIODIC, .startTimer = DL_TIMER_STOP };    /* 5 ms */
    DL_TimerG_setClockConfig(TIMG4, &c);
    DL_TimerG_initTimerMode(TIMG4, &t);
    DL_TimerG_enableInterrupt(TIMG4, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    DL_TimerG_enableClock(TIMG4);
    ISR_Register(TIM14_IRQn, timer14_interrupt);
    DL_TimerG_startCounter(TIMG4);
}
void timer14_interrupt(void) { PORT_ToggleBit(PORTB, PIN8); }

/* --- TIM16: input capture on CH1, frequency from two rising edges (PA10 here) --- */
uint16_t uhIC3ReadValue1, uhIC3ReadValue2, uhCaptureNumber;
uint32_t uwCapture, uwTIM16Freq;
void TIM16_Config(void)
{
    timg_power(TIMG4);
    DL_GPIO_initPeripheralInputFunction(IOMUX_PINCM11, IOMUX_PINCM11_PF_TIMG4_CCP0);   /* PA10 */
    DL_TimerG_CaptureConfig cap = { .captureMode = DL_TIMER_CAPTURE_MODE_EDGE_TIME, .period = 0xFFFF, .startTimer = DL_TIMER_STOP,
        .edgeCaptMode = DL_TIMER_CAPTURE_EDGE_DETECTION_MODE_RISING, .inputChan = DL_TIMER_INPUT_CHAN_0, .inputInvMode = DL_TIMER_CC_INPUT_INV_NOINVERT };
    DL_TimerG_setClockConfig(TIMG4, (DL_TimerG_ClockConfig *) &clk_bus1);
    DL_TimerG_initCaptureMode(TIMG4, &cap);
    DL_TimerG_enableInterrupt(TIMG4, DL_TIMERG_INTERRUPT_CC0_DN_EVENT);
    DL_TimerG_enableClock(TIMG4);
    ISR_Register(TIM16_IRQn, timer16_interrupt);
    DL_TimerG_startCounter(TIMG4);
}
void timer16_interrupt(void)
{
    uint16_t v = DL_TimerG_getCaptureCompareValue(TIMG4, DL_TIMER_CC_0_INDEX);
    if (uhCaptureNumber == 0) { uhIC3ReadValue1 = v; uhCaptureNumber = 1; }
    else {
        uhIC3ReadValue2 = v;
        uwCapture = (uint16_t) (uhIC3ReadValue1 - uhIC3ReadValue2);    /* TIMG counts down */
        uwTIM16Freq = uwCapture ? SystemCoreClock / uwCapture : 0;
        uhCaptureNumber = 0;
    }
}

/* --- TIM17: PWM with dead time and a break input in the original. TIMG2 on the MSPM0L1306 has no
 * dead band and no fault input (the MSPM0G parts' TIMA has both), so this is a plain 17.57 kHz PWM. --- */
void TIM17_Config(void)
{
    timg_power(TIMG2);
    DL_GPIO_initPeripheralOutputFunction(IOMUX_PINCM4, IOMUX_PINCM4_PF_TIMG2_CCP0);   /* PA3 */
    DL_GPIO_enableOutput(GPIOA, DL_GPIO_PIN_3);
    uint32_t period = SystemCoreClock / 17570u - 1;
    DL_TimerG_PWMConfig pwm = { .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN, .period = period, .isTimerWithFourCC = false, .startTimer = DL_TIMER_STOP };
    DL_TimerG_setClockConfig(TIMG2, (DL_TimerG_ClockConfig *) &clk_bus1);
    DL_TimerG_initPWMMode(TIMG2, &pwm);
    DL_TimerG_setCaptureCompareValue(TIMG2, period / 2, DL_TIMER_CC_0_INDEX);
    DL_TimerG_setCCPDirection(TIMG2, DL_TIMER_CC0_OUTPUT);
    DL_TimerG_enableClock(TIMG2);
    DL_TimerG_startCounter(TIMG2);
}
void timer17_interrupt(void) { }
