#include <string.h>
/* Serial commissioning of the TI FOC port. Never starts the motor state machine.
 * Keeps DRVOFF asserted through TI's comparator and ADC-offset calibration.
 * 'z' enables with all inputs low; u/j/0..5/m request bounded active vectors.
 * Its timeout is local to the MCU, independent of the host serial connection. */
#include "main.h"
#include "system_config.h"
#include "commissioning.h"
#include "commissioning_guard.h"
#include "haptic_core.h"
#include "pi.h"
#include "hall_cal.h"
#include "clarke.h"
#include "park.h"
#include "ipark.h"
#include "svgen.h"
#define FOC_DATA (*motorVars_M1.halInterface->halFOCDataHandle)

static void text(const char *s) {
    while (*s) {
        while (DL_UART_isTXFIFOFull(SERIAL_DATA_PLOT_INST)) {}
        DL_UART_transmitData(SERIAL_DATA_PLOT_INST, (uint8_t)*s++);
    }
}
static void number(int32_t v) {
    char s[12]; unsigned n = 0;
    if (v < 0) { text("-"); v = -v; }
    do { s[n++] = '0' + v % 10; v /= 10; } while (v);
    while (n) { char t[2] = {s[--n], 0}; text(t); }
}
static void kv(const char *s, int32_t v) { text(s); number(v); text(" "); }
static int fault(void) {
    return !DL_GPIO_readPins(MTR1_GATE_DRIVER_nFAULT_GPIO_PORT, MTR1_GATE_DRIVER_nFAULT_GPIO_PIN);
}
static void off(void) {
    DL_GPIO_setPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT, MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN);
    motorVars_M1.inputCtrl.flagEnableRunAndIdentify = false;
    HAL_disablePWM(&halFOCConfig_M1.pwmCfg, &FOC_DATA.pwmData);
}
static uint32_t raw_mv(uint32_t raw) { return raw * 3300U / 4095U; }
static void sample(uint32_t *vm, uint32_t *a, uint32_t *b, uint32_t *c) {
    *vm = (uint32_t)(FOC_DATA.adcData.VdcBus_V * 1000.0f);
    *a = raw_mv(DL_ADC_readResult(MTR1_IU_SEN_ADC_RESULT_INST, MTR1_IU_SEN));
    *b = raw_mv(DL_ADC_readResult(MTR1_IV_SEN_ADC_RESULT_INST, MTR1_IV_SEN));
    *c = raw_mv(DL_ADC_readResult(MTR1_IW_SEN_ADC_RESULT_INST, MTR1_IW_SEN));
}
static uint32_t pwm_pads(void) {
    uint32_t a = DL_GPIO_readPins(GPIO0, DL_GPIO_PIN(7)|DL_GPIO_PIN(2)|DL_GPIO_PIN(25));
    uint32_t b = DL_GPIO_readPins(GPIO1, DL_GPIO_PIN(3)|DL_GPIO_PIN(1)|DL_GPIO_PIN(2));
    return (!!(b & DL_GPIO_PIN(3))) | ((!!(a & DL_GPIO_PIN(7))) << 1) |
           ((!!(b & DL_GPIO_PIN(1))) << 2) | ((!!(a & DL_GPIO_PIN(2))) << 3) |
           ((!!(b & DL_GPIO_PIN(2))) << 4) | ((!!(a & DL_GPIO_PIN(25))) << 5);
}
static void print_status(void) {
    uint32_t vm, a, b, c; sample(&vm, &a, &b, &c);
    kv("fault=", fault());
    kv("off_pad=", !!DL_GPIO_readPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT, MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN));
    kv("sleep_pad=", !!DL_GPIO_readPins(MTR1_GATE_DRIVER_NSLEEP_GPIO_PORT, MTR1_GATE_DRIVER_NSLEEP_GPIO_PIN));
    kv("pwm_pads=", pwm_pads());
    kv("trip=", DL_MCPWM_getTripZoneFlagStatus((MCPWM_Regs *)halFOCConfig_M1.pwmCfg.pwmHandle));
    kv("cal=", motorVars_M1.statusVars.flagOffsetCalibDone);
    kv("state=", motorVars_M1.outVars.motorState);
    kv("isr=", systemVars.timerCntFOCCtrlISR);
    kv("vm_mV=", vm); kv("a_mV=", a); kv("b_mV=", b); kv("c_mV=", c);
    for (unsigned i=0; i<3; ++i) kv("offset_counts=", (int32_t)FOC_DATA.adcData.offset_I_ad.value[i]);
    text("\r\n");
}
/* Change only PWM logic inputs while DRVOFF stays asserted. Clear historical
 * trips once, with all inputs forced low; never disable their live sources.
 * Stop at the first fault or pad mismatch. No enable operation is present. */
static void disabled_trace(unsigned firstHigh) {
    MCPWM_Regs *pwm = (MCPWM_Regs *)halFOCConfig_M1.pwmCfg.pwmHandle;
    const uint32_t faultTrips = DL_MCPWM_TZ_SIGNAL_OST2 | DL_MCPWM_TZ_SIGNAL_OST3;
    const uint32_t masks[] = {0U, 2U, 8U, 32U, firstHigh, 4U, 16U, 33U};
    const DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE outputs[] = {
        DL_MCPWM_AQ_OUTPUT_1A, DL_MCPWM_AQ_OUTPUT_1B,
        DL_MCPWM_AQ_OUTPUT_2A, DL_MCPWM_AQ_OUTPUT_2B,
        DL_MCPWM_AQ_OUTPUT_3A, DL_MCPWM_AQ_OUTPUT_3B};
    struct { uint32_t mask, pads, offpad, fault, trips, firstCycles; } rows[8];
    unsigned count = 0;
    uint32_t vm, a, b, c; off(); sample(&vm, &a, &b, &c);
    if (!motorVars_M1.statusVars.flagOffsetCalibDone || pwm_pads() ||
        !DL_GPIO_readPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT, MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN) ||
        !commissioning_ready(fault(), vm, a, b, c)) {
        text("disabled trace rejected: fault/calibration/feedback/pads\r\n"); return;
    }
    __disable_irq();
    DL_MCPWM_disableADCTrigger(pwm, DL_MCPWM_SOC_A);
    /* MOTOR_STOP still asserted and all software forces low at this point. */
    DL_MCPWM_clearTripZoneFlagStatus(pwm, faultTrips);
    DL_Common_delayCycles(2000U);
    if (fault() || (DL_MCPWM_getTripZoneFlagStatus(pwm) & faultTrips)) {
        off(); DL_MCPWM_enableADCTrigger(pwm, DL_MCPWM_SOC_A); __enable_irq();
        text("disabled trace rejected: live trip\r\n"); print_status(); return;
    }
    DL_XBAR_setInputXBAR(DL_XBAR_INPUT1, DL_XBAR_DRIVE_HIGH);
    DL_MCPWM_clearTripZoneFlagStatus(pwm, DL_MCPWM_TZ_SIGNAL_OST1);
    DL_MCPWM_clearInterrupt(pwm, DL_MCPWM_INT_TZ_OST);
    for (unsigned j=0; j<8; ++j) {
        HAL_forcePWMLow(&halFOCConfig_M1.pwmCfg);
        DL_Common_delayCycles(2000U);
        uint32_t start = DWT->CYCCNT, first = 0;
        for (unsigned i=0; i<6; ++i)
            if (masks[j] & (1U << i))
                DL_MCPWM_setActionQualifierSWAction(pwm, outputs[i], DL_MCPWM_AQ_SW_CONTINUOUS_HIGH);
        uint32_t offpad = 1U, f = 0U, trips = 0U, pads = 0U;
        while ((uint32_t)(DWT->CYCCNT-start) < 4000U) {
            offpad &= !!DL_GPIO_readPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT, MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN);
            f = fault(); trips = DL_MCPWM_getTripZoneFlagStatus(pwm) & faultTrips;
            pads = pwm_pads();
            if (f || trips || !offpad) { first = DWT->CYCCNT-start; break; }
        }
        HAL_forcePWMLow(&halFOCConfig_M1.pwmCfg);
        rows[count].mask=masks[j]; rows[count].pads=pads; rows[count].offpad=offpad;
        rows[count].fault=f; rows[count].trips=trips; rows[count].firstCycles=first; ++count;
        if (f || trips || !offpad || pads!=masks[j]) break;
    }
    off();
    DL_MCPWM_enableADCTrigger(pwm, DL_MCPWM_SOC_A); __enable_irq();
    for (unsigned j=0; j<count; ++j) {
        kv("disabled_mask=", rows[j].mask); kv("pads=", rows[j].pads);
        kv("off_min=", rows[j].offpad); kv("fault=", rows[j].fault);
        kv("trips=", rows[j].trips); kv("first_event_cycles=", rows[j].firstCycles); text("\r\n");
    }
    print_status();
}
/* Weak internal pulls probe source impedance; DAC and GPIO output stay off.
 * ADC2 SOC2 is temporarily redirected from phase voltage to PA4/AIN10.
 * This tests the auxiliary lead, not a claimed ILIMIT voltage. */
static uint32_t auxiliary_pull_probe(bool keepDac) {
    MCPWM_Regs *pwm = (MCPWM_Regs *)halFOCConfig_M1.pwmCfg.pwmHandle;
    ADC_Regs *adc = ADC2;
    const uint32_t pulls[] = {0U, IOMUX_PINCM_PIPU_ENABLE, IOMUX_PINCM_PIPD_ENABLE, 0U};
    uint32_t means[4]={0}, lows[4]={4095,4095,4095,4095}, highs[4]={0}, counts[4]={0};
    const unsigned modes = keepDac ? 1U : 4U;
    off(); if (!keepDac) DL_SYSCTL_disableDACOUT();
    uint32_t pin = IOMUX->SECCFG.PINCM[IOMUX_PINCM_PA4];
    __disable_irq(); DL_MCPWM_disableADCTrigger(pwm, DL_MCPWM_SOC_A);
    uint32_t waitStart = DWT->CYCCNT;
    while (DL_ADC_isBusy(adc) && (uint32_t)(DWT->CYCCNT-waitStart)<20000U) {}
    if (DL_ADC_isBusy(adc)) {
        off(); DL_MCPWM_enableADCTrigger(pwm, DL_MCPWM_SOC_A); __enable_irq();
        text("aux probe rejected: ADC busy\r\n"); return UINT32_MAX;
    }
    uint32_t savedSoc = adc->ADCSOC2CTL, savedSeq = adc->ADCSEQ1CONFIG;
    DL_ADC_selectSOCChannel(adc, DL_ADC_SOC_NUMBER2, DL_ADC_CH_ADCIN18);
    DL_ADC_setupSequencer(adc, DL_ADC_SEQ_NUMBER1, 1472U, DL_ADC_TRIGGER_SOFTWARE, DL_ADC_SOC_NUMBER0);
    for (unsigned mode=0; mode<modes; ++mode) {
        IOMUX->SECCFG.PINCM[IOMUX_PINCM_PA4] = IOMUX_PINCM_PC_UNCONNECTED | pulls[mode];
        DL_Common_delayCycles(1000000U);
        for (unsigned n=0; n<32; ++n) {
            DL_ADC_clearInterruptStatus(adc, DL_ADC_INT_NUMBER1);
            DL_ADC_forceSequencer(adc, DL_ADC_SEQ_NUMBER1);
            waitStart = DWT->CYCCNT;
            while (!DL_ADC_getRawInterruptStatus(adc, DL_ADC_INT_NUMBER1) &&
                   (uint32_t)(DWT->CYCCNT-waitStart)<20000U) {}
            if (!DL_ADC_getRawInterruptStatus(adc, DL_ADC_INT_NUMBER1)) break;
            uint32_t raw=DL_ADC_readResult(ADC2RESULT, DL_ADC_SOC_NUMBER2);
            means[mode]+=raw; ++counts[mode];
            if (raw<lows[mode]) lows[mode]=raw;
            if (raw>highs[mode]) highs[mode]=raw;
        }
    }
    adc->ADCSOC2CTL=savedSoc; adc->ADCSEQ1CONFIG=savedSeq; IOMUX->SECCFG.PINCM[IOMUX_PINCM_PA4]=pin;
    off(); DL_MCPWM_enableADCTrigger(pwm, DL_MCPWM_SOC_A); __enable_irq();
    for (unsigned mode=0; mode<modes; ++mode) {
        kv("aux_pull_mode=",mode); kv("count=",counts[mode]);
        kv("mean_mV=",counts[mode] ? raw_mv(means[mode]/counts[mode]) : -1);
        kv("min_mV=",raw_mv(lows[mode])); kv("max_mV=",raw_mv(highs[mode])); text("\r\n");
    }
    print_status();
    return counts[0] ? raw_mv(means[0]/counts[0]) : UINT32_MAX;
}
static void restore_reference(uint32_t pin, uint16_t low) {
    off(); DL_SYSCTL_disableDACOUT(); DL_CMPSSLITE_setDACValueLow(CMPSS2, low);
    IOMUX->SECCFG.PINCM[IOMUX_PINCM_PA4]=pin;
}
/* Two-second six-step recovery trial. Software-forced PWM keeps the TI FOC
 * state machine out of this experiment. All hardware trip sources stay live. */
static uint32_t read_hall(void) {
    return (!!DL_GPIO_readPins(MTR1_HALL_A_PORT,MTR1_HALL_A_PIN)<<2) |
           (!!DL_GPIO_readPins(MTR1_HALL_B_PORT,MTR1_HALL_B_PIN)<<1) |
           !!DL_GPIO_readPins(MTR1_HALL_C_PORT,MTR1_HALL_C_PIN);
}
static void bounded_sixstep(unsigned duty, bool openLoop, int requestedDirection, unsigned durationMs, unsigned profile) {
    MCPWM_Regs *pwm=(MCPWM_Regs *)halFOCConfig_M1.pwmCfg.pwmHandle;
    const uint32_t trips=DL_MCPWM_TZ_SIGNAL_OST2|DL_MCPWM_TZ_SIGNAL_OST3;
    const DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE highs[]={DL_MCPWM_AQ_OUTPUT_1A,DL_MCPWM_AQ_OUTPUT_1A,DL_MCPWM_AQ_OUTPUT_2A,DL_MCPWM_AQ_OUTPUT_2A,DL_MCPWM_AQ_OUTPUT_3A,DL_MCPWM_AQ_OUTPUT_3A};
    const DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE lows[]={DL_MCPWM_AQ_OUTPUT_2B,DL_MCPWM_AQ_OUTPUT_3B,DL_MCPWM_AQ_OUTPUT_3B,DL_MCPWM_AQ_OUTPUT_1B,DL_MCPWM_AQ_OUTPUT_1B,DL_MCPWM_AQ_OUTPUT_2B};
    /* Rev16 slow field sequence held these Hall states at the end of each
     * vector:0->3,1->1,2->5,3->4,4->6,5->2. Advance one vector for torque.
     * Candidate derived from this wiring, not the prior spin session's table. */

    uint32_t savedPin=IOMUX->SECCFG.PINCM[IOMUX_PINCM_PA4];
    uint16_t savedLow=DL_CMPSSLITE_getDACValueLow(CMPSS2);
    off();
    DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM_PA4);
    DL_CMPSSLITE_setDACValueLow(CMPSS2,2048U);
    DL_SYSCTL_enableDACOUT(DL_SYSCTL_DACOUTEN_CMPSS2_DACL);
    DEVICE_DELAY_MS(5);
    uint32_t reference=auxiliary_pull_probe(true),vm,a,b,c;
    sample(&vm,&a,&b,&c);
    unsigned h=read_hall();
    if(durationMs>6000U || durationMs<100U || profile>2U ||
       (requestedDirection!=1 && requestedDirection!=-1) || duty>15U || duty<5U || reference<1670U || reference>1730U ||
       !motorVars_M1.statusVars.flagOffsetCalibDone || pwm_pads() ||
       !DL_GPIO_readPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT,MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN) ||
       !commissioning_ready(fault(),vm,a,b,c) || h==0U || h==7U ||
       (DL_MCPWM_getTripZoneFlagStatus(pwm)&trips)) {
        restore_reference(savedPin,savedLow); text("sixstep rejected: preflight\r\n"); print_status(); return;
    }
    uint32_t baseline[3]={a,b,c},peak[3]={0,0,0};
    unsigned initial=h,candidate=h,stable=0,edges=0,skips=0,reason=0,periods=0,limited=0;
    uint32_t lastDelta=0,firstEdgeUs=0,minVm=vm,maxVm=vm,maxPeriodCycles=0;
    struct {int net;unsigned edges;unsigned limited;unsigned duty;} bins[60]={{0}};
    unsigned binCount=0;
    int net=0; uint32_t maxRun=0,consecutive=0; int direction=0;
    struct {uint32_t us; unsigned h; int net;} trace[128]; unsigned traceN=0;
    __disable_irq(); DL_MCPWM_disableADCTrigger(pwm,DL_MCPWM_SOC_A);
    uint32_t drain=DWT->CYCCNT;
    while(DL_ADC_isBusy(ADC2) && (uint32_t)(DWT->CYCCNT-drain)<20000U) {}
    if(DL_ADC_isBusy(ADC2)) {
        off(); DL_MCPWM_enableADCTrigger(pwm,DL_MCPWM_SOC_A); __enable_irq();
        restore_reference(savedPin,savedLow); text("sixstep rejected: ADC busy\r\n"); return;
    }
    DL_XBAR_setInputXBAR(DL_XBAR_INPUT1,DL_XBAR_DRIVE_HIGH);
    DL_MCPWM_clearTripZoneFlagStatus(pwm,DL_MCPWM_TZ_SIGNAL_OST1);
    DL_MCPWM_clearInterrupt(pwm,DL_MCPWM_INT_TZ_OST);
    DL_GPIO_clearPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT,MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN);
    uint32_t begin=DWT->CYCCNT,lastEdge=begin;
    while((uint32_t)(DWT->CYCCNT-begin)<durationMs*200000U && !reason) {
        uint32_t period=DWT->CYCCNT;
        if(fault() || (DL_MCPWM_getTripZoneFlagStatus(pwm)&trips)) {reason=1;break;}
        unsigned observed=read_hall();
        /* Deliberate invalid Hall observation tests the normal stop path. */
        if(profile==2U && (uint32_t)(period-begin)>=100000000U) observed=0U;
        if(observed==candidate) ++stable; else {candidate=observed;stable=1;}
        if(stable>=3U && candidate!=h) {
            if(candidate==0U || candidate==7U) {reason=2;break;}
            int step=commissioning_hall_step(h,candidate);
            ++edges; net+=step; if(!firstEdgeUs) firstEdgeUs=(period-begin)/200U;
            if(!step) {++skips;consecutive=0;direction=0;}
            else {consecutive=step==direction ? consecutive+1U : 1U;direction=step;if(consecutive>maxRun)maxRun=consecutive;}
            h=candidate;lastEdge=period;
            if(traceN<128U) {trace[traceN].us=(period-begin)/200U;trace[traceN].h=h;trace[traceN++].net=net;}
            if(skips>=3U) {reason=3;break;}
        }
        if((uint32_t)(period-lastEdge)>100000000U) {reason=4;break;}
        /* Slow open-loop field:200ms alignment, then50ms per sector. */
        uint32_t age=(uint32_t)(period-begin);
        unsigned v=openLoop ? (age<40000000U ? 0U : ((age-40000000U)/10000000U+1U)%6U) : commissioning_vector(h,requestedDirection);
        if(v>=6U) {reason=2;break;}
        unsigned activeDuty=profile==1U ? (age<400000000U ? 8U : (age<800000000U ? 15U : 8U)) : duty;
        HAL_forcePWMLow(&halFOCConfig_M1.pwmCfg);
        DL_MCPWM_setActionQualifierSWAction(pwm,lows[v],DL_MCPWM_AQ_SW_CONTINUOUS_HIGH);
        uint32_t sinkOn=DWT->CYCCNT;
        DL_Common_delayCycles(200U);
        uint32_t on=DWT->CYCCNT;
        DL_ADC_clearInterruptStatus(ADC2,DL_ADC_INT_NUMBER1);
        /* Skip the next source pulse above the250mV current target.
         * The independent400mV abort and hardware trips remain active. */
        if(lastDelta<250U) DL_MCPWM_setActionQualifierSWAction(pwm,highs[v],DL_MCPWM_AQ_SW_CONTINUOUS_HIGH);
        else ++limited;
        while((uint32_t)(DWT->CYCCNT-on)<activeDuty*100U) {
            if(fault() || (DL_MCPWM_getTripZoneFlagStatus(pwm)&trips)) {reason=1;break;}

        }
        DL_MCPWM_setActionQualifierSWAction(pwm,highs[v],DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
        if(reason) break;
        /* CSA valid even at the slowest SLEW setting: retain sink low side
         * and sample after7us, including source-off recirculation periods. */
        while((uint32_t)(DWT->CYCCNT-sinkOn)<1400U) {
            if(fault() || (DL_MCPWM_getTripZoneFlagStatus(pwm)&trips)) {reason=1;break;}
        }
        if(reason) break;
        DL_ADC_forceSequencer(MTR1_VDC_SEN_ADC_INST,DL_ADC_SEQ_NUMBER1);
        DL_ADC_forceSequencer(ADC1,DL_ADC_SEQ_NUMBER1);
        DL_ADC_forceSequencer(ADC2,DL_ADC_SEQ_NUMBER1);
        uint32_t wait=DWT->CYCCNT;
        while(!DL_ADC_getRawInterruptStatus(ADC2,DL_ADC_INT_NUMBER1) &&
              (uint32_t)(DWT->CYCCNT-wait)<2000U) {
            if(fault() || (DL_MCPWM_getTripZoneFlagStatus(pwm)&trips)) {reason=1;break;}
        }
        if(reason) break;
        if(!DL_ADC_getRawInterruptStatus(ADC2,DL_ADC_INT_NUMBER1)) {reason=5;break;}
        uint32_t values[]={raw_mv(DL_ADC_readResult(MTR1_IU_SEN_ADC_RESULT_INST,MTR1_IU_SEN)),raw_mv(DL_ADC_readResult(MTR1_IV_SEN_ADC_RESULT_INST,MTR1_IV_SEN)),raw_mv(DL_ADC_readResult(MTR1_IW_SEN_ADC_RESULT_INST,MTR1_IW_SEN))};
        lastDelta=0;
        for(unsigned i=0;i<3;i++) {
            uint32_t delta=values[i]>baseline[i] ? values[i]-baseline[i] : baseline[i]-values[i];
            if(delta>peak[i])peak[i]=delta;
            if(delta>lastDelta)lastDelta=delta;
            if(delta>400U)reason=6;
        }
        vm=raw_mv(DL_ADC_readResult(MTR1_VDC_SEN_ADC_RESULT_INST,MTR1_VDC_SEN))*14904U/604U;
        if(vm<9000U || vm>15000U)reason=7;
        if(vm<minVm)minVm=vm; if(vm>maxVm)maxVm=vm;
        ++periods;
        while(!reason && (uint32_t)(DWT->CYCCNT-period)<10000U) {
            if(fault() || (DL_MCPWM_getTripZoneFlagStatus(pwm)&trips))reason=1;
            if(!DL_UART_isRXFIFOEmpty(SERIAL_DATA_PLOT_INST) && DL_UART_receiveData(SERIAL_DATA_PLOT_INST)=='s')reason=8;
        }
        uint32_t periodCycles=(uint32_t)(DWT->CYCCNT-period);
        if(periodCycles>maxPeriodCycles)maxPeriodCycles=periodCycles;
        unsigned bin=age/20000000U;
        if(bin<60U) {bins[bin].net=net;bins[bin].edges=edges;bins[bin].limited=limited;bins[bin].duty=activeDuty;binCount=bin+1U;}
    }
    off(); uint32_t elapsed=(DWT->CYCCNT-begin)/200U;
    DL_MCPWM_enableADCTrigger(pwm,DL_MCPWM_SOC_A); __enable_irq();
    restore_reference(savedPin,savedLow);
    kv("requested_direction=",requestedDirection);kv("duration_ms=",durationMs);kv("profile=",profile);kv("first_edge_us=",firstEdgeUs);kv("min_vm_mV=",minVm);kv("max_vm_mV=",maxVm);kv("max_period_cycles=",maxPeriodCycles);text("\r\n");
    kv("open_loop=",openLoop);kv("sixstep_duty=",duty);kv("elapsed_us=",elapsed);kv("reason=",reason);kv("initial_hall=",initial);kv("final_hall=",h);kv("edges=",edges);kv("net_sectors=",net);kv("longest_ordered=",maxRun);kv("skips=",skips);kv("periods=",periods);kv("limited_periods=",limited);kv("last_vm_mV=",vm);text("\r\n");
    for(unsigned i=0;i<3;i++) {kv("raw_channel=",i);kv("peak_delta_mV=",peak[i]);text("\r\n");}
    for(unsigned i=0;i<traceN;i++) {kv("edge_us=",trace[i].us);kv("hall=",trace[i].h);kv("net=",trace[i].net);text("\r\n");}
    for(unsigned i=0;i<binCount;i++) {kv("bin_end_ms=",(i+1U)*100U);kv("bin_net=",bins[i].net);kv("bin_edges=",bins[i].edges);kv("bin_limited=",bins[i].limited);kv("bin_duty=",bins[i].duty);text("\r\n");}
    print_status();
}

/* Haptic knob (key 'H'). Same drive path, guards and abort rules as bounded_sixstep, with three differences:
 * the torque command comes from haptic_core.h (a coarse spring to the nearest detent, zero current at a detent),
 * the 500 ms no-edge abort is off because a knob rests, and the run is capped at HAPTIC_SECONDS_MAX seconds.
 * The host changes detents and strength while it runs ("D12\n", "W5\n") and reads one telemetry line every 100 ms
 * ("H,<ms>,<net sectors>,<angle deg>,<hall>,<duty>,<detents>,<strength>"). 's' stops. Duty never exceeds
 * HAPTIC_DUTY_MAX and the 250 mV skip, 400 mV abort and every hardware trip stay live. */
static void bounded_haptic(void) {
    MCPWM_Regs *pwm=(MCPWM_Regs *)halFOCConfig_M1.pwmCfg.pwmHandle;
    const uint32_t trips=DL_MCPWM_TZ_SIGNAL_OST2|DL_MCPWM_TZ_SIGNAL_OST3;
    const DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE highs[]={DL_MCPWM_AQ_OUTPUT_1A,DL_MCPWM_AQ_OUTPUT_1A,DL_MCPWM_AQ_OUTPUT_2A,DL_MCPWM_AQ_OUTPUT_2A,DL_MCPWM_AQ_OUTPUT_3A,DL_MCPWM_AQ_OUTPUT_3A};
    const DL_MCPWM_ACTION_QUALIFIER_OUTPUT_MODULE lows[]={DL_MCPWM_AQ_OUTPUT_2B,DL_MCPWM_AQ_OUTPUT_3B,DL_MCPWM_AQ_OUTPUT_3B,DL_MCPWM_AQ_OUTPUT_1B,DL_MCPWM_AQ_OUTPUT_1B,DL_MCPWM_AQ_OUTPUT_2B};
    uint32_t savedPin=IOMUX->SECCFG.PINCM[IOMUX_PINCM_PA4];
    uint16_t savedLow=DL_CMPSSLITE_getDACValueLow(CMPSS2);
    off();
    DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM_PA4);
    DL_CMPSSLITE_setDACValueLow(CMPSS2,2048U);
    DL_SYSCTL_enableDACOUT(DL_SYSCTL_DACOUTEN_CMPSS2_DACL);
    DEVICE_DELAY_MS(5);
    uint32_t reference=auxiliary_pull_probe(true),vm,a,b,c;
    sample(&vm,&a,&b,&c);
    unsigned h=read_hall();
    if(reference<1670U || reference>1730U || !motorVars_M1.statusVars.flagOffsetCalibDone || pwm_pads() ||
       !DL_GPIO_readPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT,MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN) ||
       !commissioning_ready(fault(),vm,a,b,c) || h==0U || h==7U || (DL_MCPWM_getTripZoneFlagStatus(pwm)&trips)) {
        restore_reference(savedPin,savedLow); text("haptic rejected: preflight\r\n"); print_status(); return;
    }
    uint32_t baseline[3]={a,b,c};
    unsigned candidate=h,stable=0,reason=0,detents=12U,strength=5U,duty=0U,edges=0,skips=0,limited=0;
    uint32_t lastDelta=0,lastTelem=0,maxPeriodCycles=0;
    int net=0;
    char pend=0; unsigned pendVal=0; /* host parameter line being read */
    char tx[64]; unsigned txHead=0,txTail=0;
    __disable_irq(); DL_MCPWM_disableADCTrigger(pwm,DL_MCPWM_SOC_A);
    uint32_t drain=DWT->CYCCNT;
    while(DL_ADC_isBusy(ADC2) && (uint32_t)(DWT->CYCCNT-drain)<20000U) {}
    if(DL_ADC_isBusy(ADC2)) {
        off(); DL_MCPWM_enableADCTrigger(pwm,DL_MCPWM_SOC_A); __enable_irq();
        restore_reference(savedPin,savedLow); text("haptic rejected: ADC busy\r\n"); return;
    }
    DL_XBAR_setInputXBAR(DL_XBAR_INPUT1,DL_XBAR_DRIVE_HIGH);
    DL_MCPWM_clearTripZoneFlagStatus(pwm,DL_MCPWM_TZ_SIGNAL_OST1);
    DL_MCPWM_clearInterrupt(pwm,DL_MCPWM_INT_TZ_OST);
    DL_GPIO_clearPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT,MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN);
    uint32_t begin=DWT->CYCCNT;
    while((uint32_t)(DWT->CYCCNT-begin)<HAPTIC_SECONDS_MAX*200000000U && !reason) {
        uint32_t period=DWT->CYCCNT;
        if(fault() || (DL_MCPWM_getTripZoneFlagStatus(pwm)&trips)) {reason=1;break;}
        unsigned observed=read_hall();
        if(observed==candidate) ++stable; else {candidate=observed;stable=1;}
        if(stable>=3U && candidate!=h) {
            if(candidate==0U || candidate==7U) {reason=2;break;}
            int step=commissioning_hall_step(h,candidate);
            ++edges; net+=step;
            if(!step) {if(++skips>=3U) {reason=3;break;}}
            h=candidate;
        }
        int dir=0; duty=haptic_command(net,detents,strength,&dir);
        HAL_forcePWMLow(&halFOCConfig_M1.pwmCfg);
        if(duty && dir) {
            unsigned v=commissioning_vector(h,dir);
            if(v>=6U) {reason=2;break;}
            DL_MCPWM_setActionQualifierSWAction(pwm,lows[v],DL_MCPWM_AQ_SW_CONTINUOUS_HIGH);
            uint32_t sinkOn=DWT->CYCCNT;
            DL_Common_delayCycles(200U);
            uint32_t on=DWT->CYCCNT;
            DL_ADC_clearInterruptStatus(ADC2,DL_ADC_INT_NUMBER1);
            if(lastDelta<250U) DL_MCPWM_setActionQualifierSWAction(pwm,highs[v],DL_MCPWM_AQ_SW_CONTINUOUS_HIGH);
            else ++limited;
            while((uint32_t)(DWT->CYCCNT-on)<duty*100U) {
                if(fault() || (DL_MCPWM_getTripZoneFlagStatus(pwm)&trips)) {reason=1;break;}
            }
            DL_MCPWM_setActionQualifierSWAction(pwm,highs[v],DL_MCPWM_AQ_SW_CONTINUOUS_LOW);
            if(reason) break;
            while((uint32_t)(DWT->CYCCNT-sinkOn)<1400U) {
                if(fault() || (DL_MCPWM_getTripZoneFlagStatus(pwm)&trips)) {reason=1;break;}
            }
            if(reason) break;
            DL_ADC_forceSequencer(MTR1_VDC_SEN_ADC_INST,DL_ADC_SEQ_NUMBER1);
            DL_ADC_forceSequencer(ADC1,DL_ADC_SEQ_NUMBER1);
            DL_ADC_forceSequencer(ADC2,DL_ADC_SEQ_NUMBER1);
            uint32_t wait=DWT->CYCCNT;
            while(!DL_ADC_getRawInterruptStatus(ADC2,DL_ADC_INT_NUMBER1) && (uint32_t)(DWT->CYCCNT-wait)<2000U) {
                if(fault() || (DL_MCPWM_getTripZoneFlagStatus(pwm)&trips)) {reason=1;break;}
            }
            if(reason) break;
            if(!DL_ADC_getRawInterruptStatus(ADC2,DL_ADC_INT_NUMBER1)) {reason=5;break;}
            uint32_t values[]={raw_mv(DL_ADC_readResult(MTR1_IU_SEN_ADC_RESULT_INST,MTR1_IU_SEN)),raw_mv(DL_ADC_readResult(MTR1_IV_SEN_ADC_RESULT_INST,MTR1_IV_SEN)),raw_mv(DL_ADC_readResult(MTR1_IW_SEN_ADC_RESULT_INST,MTR1_IW_SEN))};
            lastDelta=0;
            for(unsigned i=0;i<3;i++) {
                uint32_t delta=values[i]>baseline[i] ? values[i]-baseline[i] : baseline[i]-values[i];
                if(delta>lastDelta)lastDelta=delta;
                if(delta>400U)reason=6;
            }
            vm=raw_mv(DL_ADC_readResult(MTR1_VDC_SEN_ADC_RESULT_INST,MTR1_VDC_SEN))*14904U/604U;
            if(vm<9000U || vm>15000U)reason=7;
        } else {
            lastDelta=0;                                  /* coasting: nothing to limit */
        }
        /* Host: stop, or a parameter line ("D12", "W5"); at most 4 bytes read per period. */
        for(unsigned k=0;k<4U && !DL_UART_isRXFIFOEmpty(SERIAL_DATA_PLOT_INST);k++) {
            int ch=DL_UART_receiveData(SERIAL_DATA_PLOT_INST);
            if(ch=='s') reason=8;
            else if(ch=='D' || ch=='W') {pend=(char)ch;pendVal=0U;}
            else if(pend && ch>='0' && ch<='9' && pendVal<100U) pendVal=pendVal*10U+(unsigned)(ch-'0');
            else if(pend && (ch=='\n' || ch=='\r')) {
                if(pend=='D' && haptic_detents_ok(pendVal)) detents=pendVal;
                if(pend=='W' && pendVal>=1U && pendVal<=10U) strength=pendVal;
                pend=0;
            }
        }
        /* Telemetry every 100 ms, queued and sent one byte per period so a print never stalls the drive. */
        uint32_t ageMs=(uint32_t)(period-begin)/200000U;
        if(ageMs>=lastTelem+100U && txHead==txTail) {
            lastTelem=ageMs;
            int deg=(int)(((net%HAPTIC_SECTORS)+HAPTIC_SECTORS)%HAPTIC_SECTORS)*15;
            int vals[7]={(int)ageMs,net,deg,(int)h,(int)duty,(int)detents,(int)strength};
            unsigned n=0; tx[n++]='H';
            for(unsigned i=0;i<7U;i++) {
                tx[n++]=',';
                int v=vals[i]; if(v<0){tx[n++]='-';v=-v;}
                char dg[11];unsigned m=0; do{dg[m++]=(char)('0'+v%10);v/=10;}while(v);
                while(m && n<60U) tx[n++]=dg[--m];
            }
            tx[n++]='\n'; txTail=n; txHead=0;
        }
        if(txHead<txTail && !DL_UART_isTXFIFOFull(SERIAL_DATA_PLOT_INST)) DL_UART_transmitData(SERIAL_DATA_PLOT_INST,(uint8_t)tx[txHead++]);
        if(txHead>=txTail) {txHead=0;txTail=0;}
        while(!reason && (uint32_t)(DWT->CYCCNT-period)<10000U) {
            if(fault() || (DL_MCPWM_getTripZoneFlagStatus(pwm)&trips))reason=1;
        }
        uint32_t periodCycles=(uint32_t)(DWT->CYCCNT-period);
        if(periodCycles>maxPeriodCycles)maxPeriodCycles=periodCycles;
    }
    off(); uint32_t elapsed=(DWT->CYCCNT-begin)/200U;
    DL_MCPWM_enableADCTrigger(pwm,DL_MCPWM_SOC_A); __enable_irq();
    restore_reference(savedPin,savedLow);
    text("\r\nhaptic done ");kv("reason=",reason);kv("elapsed_us=",elapsed);kv("net_sectors=",net);kv("edges=",edges);kv("skips=",skips);kv("limited=",limited);kv("max_period_cycles=",maxPeriodCycles);text("\r\n");
    print_status();
}

/* Exercise the TI complementary-PWM/ADC path independently of its state machine.
 * mode0: zero voltage; mode1: small stationary alpha voltage; mode2: rotating sine.
 * ADC2 completion is hardware-PWM synchronous. Driver off precedes all printing. */
static void bounded_native_pwm(unsigned mode) {
    MCPWM_Regs *pwm=(MCPWM_Regs *)halFOCConfig_M1.pwmCfg.pwmHandle;
    const uint32_t trips=DL_MCPWM_TZ_SIGNAL_OST2|DL_MCPWM_TZ_SIGNAL_OST3;
    uint32_t savedPin=IOMUX->SECCFG.PINCM[IOMUX_PINCM_PA4];
    uint16_t savedLow=DL_CMPSSLITE_getDACValueLow(CMPSS2);
    off(); DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM_PA4);
    DL_CMPSSLITE_setDACValueLow(CMPSS2,2048U);
    DL_SYSCTL_enableDACOUT(DL_SYSCTL_DACOUTEN_CMPSS2_DACL); DEVICE_DELAY_MS(5);
    uint32_t reference=auxiliary_pull_probe(true),vm,a,b,c;sample(&vm,&a,&b,&c);
    unsigned initialHall=read_hall();
    if(mode>4U || initialHall==0U || initialHall==7U || reference<1670U || reference>1730U || pwm_pads() ||
       !DL_GPIO_readPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT,MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN) ||
       !motorVars_M1.statusVars.flagOffsetCalibDone || !commissioning_ready(fault(),vm,a,b,c) ||
       (DL_MCPWM_getTripZoneFlagStatus(pwm)&trips) || FOC_DATA.pwmData.minCMPValue<1400U) {
        restore_reference(savedPin,savedLow);text("native rejected: preflight\r\n");print_status();return;
    }
    const uint32_t duration=(mode==2U || mode==4U) ? 1200000000U : 20000000U;
    uint32_t baseline[3]={a,b,c},peak[3]={0},samples=0,reason=0,minVm=vm,maxVm=vm;
    int current[3]={0},net=0;unsigned h=initialHall,candidate=h,stable=0,edges=0,skips=0;
    uint32_t lastDelta=0,limited=0,lastSample=0,lastEdge=0;
    float angle=0.0f,id=0.0f,iq=0.0f,idRef=0.0f,vd=0.0f,vq=0.0f;
    PI_Obj pid={0},piq={0};PARK_Obj park={0};IPARK_Obj ipark={0};
    PI_setGains(&pid,0.6f,0.04f);PI_setGains(&piq,0.6f,0.04f);
    PI_setMinMax(&pid,-0.6f,0.6f);PI_setMinMax(&piq,-0.6f,0.6f);
    static struct {uint32_t us,vm;int i[3],net,id,iq,ref,vd,vq;unsigned hall;} rows[128];unsigned count=0;
    __disable_irq();
    for(unsigned i=0;i<3;i++) FOC_DATA.pwmData.Vabc_pu.value[i]=0.0f;
    HAL_writePWMData(&halFOCConfig_M1.pwmCfg,&FOC_DATA.pwmData);
    DL_Common_delayCycles(20000U); /* Two PWM periods to load zero compares while disabled. */
    HAL_enablePWM(&halFOCConfig_M1.pwmCfg,&FOC_DATA.pwmData);
    DL_GPIO_clearPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT,MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN);
    DL_ADC_clearInterruptStatus(ADC2,DL_ADC_INT_NUMBER1);
    uint32_t begin=DWT->CYCCNT;lastSample=begin;lastEdge=begin;
    while((uint32_t)(DWT->CYCCNT-begin)<duration && !reason) {
        if(fault() || (DL_MCPWM_getTripZoneFlagStatus(pwm)&trips)) {reason=1;break;}
        if(!DL_UART_isRXFIFOEmpty(SERIAL_DATA_PLOT_INST) && DL_UART_receiveData(SERIAL_DATA_PLOT_INST)=='s') {reason=8;break;}
        uint32_t now=DWT->CYCCNT;
        if((uint32_t)(now-lastSample)>20000U) {reason=5;break;}
        if(!DL_ADC_getRawInterruptStatus(ADC2,DL_ADC_INT_NUMBER1)) continue;
        uint32_t dt=now-lastSample;lastSample=now;
        uint32_t age=now-begin;
        uint32_t values[]={raw_mv(DL_ADC_readResult(MTR1_IU_SEN_ADC_RESULT_INST,MTR1_IU_SEN)),raw_mv(DL_ADC_readResult(MTR1_IV_SEN_ADC_RESULT_INST,MTR1_IV_SEN)),raw_mv(DL_ADC_readResult(MTR1_IW_SEN_ADC_RESULT_INST,MTR1_IW_SEN))};
        DL_ADC_clearInterruptStatus(ADC2,DL_ADC_INT_NUMBER1);
        lastDelta=0;
        for(unsigned i=0;i<3;i++) {
            current[i]=(int)baseline[i]-(int)values[i]; /* nominal1V/A: mV equals mA */
            unsigned delta=current[i]<0 ? -current[i] : current[i];
            if(delta>peak[i])peak[i]=delta;if(delta>lastDelta)lastDelta=delta;
            if(delta>400U)reason=6;
        }
        vm=raw_mv(DL_ADC_readResult(MTR1_VDC_SEN_ADC_RESULT_INST,MTR1_VDC_SEN))*14904U/604U;
        if(vm<9000U || vm>15000U)reason=7;
        if(vm<minVm)minVm=vm;if(vm>maxVm)maxVm=vm;
        unsigned observed=read_hall();
        if(observed==candidate)++stable;else {candidate=observed;stable=1;}
        if(stable>=3U && (candidate==0U || candidate==7U)){reason=2;break;}
        if(stable>=3U && candidate!=h) {
            int step=commissioning_hall_step(h,candidate);net+=step;++edges;
            if(!step && ++skips>=3U){reason=3;break;}
            h=candidate;lastEdge=now;
        }
        if((mode==2U || mode==4U) && age>140000000U && (uint32_t)(now-lastEdge)>100000000U){reason=4;break;}
        float amp=mode==0U ? 0.0f : 0.012f;
        if((mode==2U || mode==4U) && age>40000000U) {
            float t=(float)(age-40000000U)/200000000.0f;
            float hz=t<2.0f ? 2.0f+2.0f*t : 6.0f;
            angle+=6.28318530718f*hz*(float)dt/200000000.0f;
            if(angle>=6.28318530718f)angle-=6.28318530718f;
            amp=0.025f+0.0025f*hz;
        }
        if(mode>=3U) {
            MATH_Vec2 ab,dq,voltdq,voltab,phasor={{cosf(angle),sinf(angle)}};
            CLARKE_run_threeInput(current[0]*0.001f,current[1]*0.001f,current[2]*0.001f,&ab.value[0],&ab.value[1]);
            PARK_setPhasor(&park,&phasor);PARK_run(&park,&ab,&dq);
            id=dq.value[0];iq=dq.value[1];
            idRef=mode==4U ? 0.15f : (age<5000000U ? 0.0f : (age<10000000U ? 0.10f : (age<15000000U ? 0.20f : 0.10f)));
            PI_run(&pid,idRef,id,&vd);PI_run(&piq,0.0f,iq,&vq);
            voltdq.value[0]=vd;voltdq.value[1]=vq;
            IPARK_setPhasor(&ipark,&phasor);IPARK_run(&ipark,&voltdq,&voltab);
            SVGEN_runCom(1000.0f/(float)vm,voltab.value[0],voltab.value[1],&FOC_DATA.pwmData.Vabc_pu.value[0],&FOC_DATA.pwmData.Vabc_pu.value[1],&FOC_DATA.pwmData.Vabc_pu.value[2]);
        } else commissioning_sine(angle,amp,FOC_DATA.pwmData.Vabc_pu.value);
        if(lastDelta>=250U) {
            ++limited;PI_setUi(&pid,0.0f);PI_setUi(&piq,0.0f);
            for(unsigned i=0;i<3;i++)FOC_DATA.pwmData.Vabc_pu.value[i]=0.0f;
        }
        HAL_writePWMData(&halFOCConfig_M1.pwmCfg,&FOC_DATA.pwmData);
        ++samples;
        /* Revision 22/23: mode 3 records 100 us rows; revision 23 starts at 49 ms so the 100 to 200 mA d-axis
         * step at 25 ms is captured at 100 us resolution (128 rows, about 12.8 ms) for test T4. */
        uint32_t spacing=(mode==2U || mode==4U) ? 10000000U : (mode==3U ? 20000U : 200000U);
        bool window=(mode!=3U) || (age>=9800000U);  /* revision 23: the 100 to 200 mA step on a bias at 50 ms */
        if(count<128U && window && (count==0U || age/200U>=rows[count-1].us+spacing/200U)) {
            rows[count].us=age/200U;rows[count].vm=vm;rows[count].net=net;rows[count].hall=h;
            for(unsigned i=0;i<3;i++)rows[count].i[i]=current[i];
            rows[count].id=id*1000.0f;rows[count].iq=iq*1000.0f;rows[count].ref=idRef*1000.0f;rows[count].vd=vd*1000.0f;rows[count].vq=vq*1000.0f;++count;
        }
    }
    off();uint32_t elapsed=(DWT->CYCCNT-begin)/200U;
    for(unsigned i=0;i<3;i++)FOC_DATA.pwmData.Vabc_pu.value[i]=0.0f;
    HAL_writePWMData(&halFOCConfig_M1.pwmCfg,&FOC_DATA.pwmData);
    __enable_irq();restore_reference(savedPin,savedLow);
    kv("native_mode=",mode);kv("elapsed_us=",elapsed);kv("reason=",reason);kv("samples=",samples);kv("net_sectors=",net);kv("edges=",edges);kv("skips=",skips);kv("limited=",limited);kv("min_vm_mV=",minVm);kv("max_vm_mV=",maxVm);kv("min_compare=",FOC_DATA.pwmData.minCMPValue);text("\r\n");
    for(unsigned i=0;i<3;i++){kv("physical_phase=",i);kv("peak_delta_mV=",peak[i]);text("\r\n");}
    for(unsigned i=0;i<count;i++){kv("sample_us=",rows[i].us);kv("ia_nominal_mA=",rows[i].i[0]);kv("ib_nominal_mA=",rows[i].i[1]);kv("ic_nominal_mA=",rows[i].i[2]);kv("vm_mV=",rows[i].vm);kv("net=",rows[i].net);kv("hall=",rows[i].hall);kv("id_mA=",rows[i].id);kv("iq_mA=",rows[i].iq);kv("id_ref_mA=",rows[i].ref);kv("vd_mV=",rows[i].vd);kv("vq_mV=",rows[i].vq);text("\r\n");}
    print_status();
}

/* Revision 31: on a hard fault the PWM peripheral keeps its last outputs while the CPU stops. Turn the gate driver
 * off (DRVOFF high, nSLEEP low) before parking, so a fault never leaves the motor energized. */
void HardFault_Handler(void) {
    DL_GPIO_setPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT, MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN);
    DL_GPIO_clearPins(MTR1_GATE_DRIVER_NSLEEP_GPIO_PORT, MTR1_GATE_DRIVER_NSLEEP_GPIO_PIN);
    for (;;) { }
}

void commissioning_stage(const char *s) { text("TI-COMMISSION-39 "); text(s); text("\r\n"); }
void commissioning_init(void) {
    DL_GPIO_setPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT, MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN);
    DL_SYSCTL_disableDACOUT();
    const uint32_t pins[] = {IOMUX_PINCM_PB18, IOMUX_PINCM_PA23, IOMUX_PINCM_PB3,
        IOMUX_PINCM_PA7, IOMUX_PINCM_PB1, IOMUX_PINCM_PA2, IOMUX_PINCM_PB2, IOMUX_PINCM_PA25};
    for (unsigned i = 0; i < sizeof(pins)/sizeof(pins[0]); ++i)
        DL_GPIO_setInputEnableBit(pins[i], DL_GPIO_INPUT_ENABLE);
    NVIC_DisableIRQ(SERIAL_DATA_PLOT_INT);
    DL_UART_disable(SERIAL_DATA_PLOT_INST);
    DL_UART_configBaudRate(SERIAL_DATA_PLOT_INST, SERIAL_DATA_PLOT_INST_FREQUENCY, 115200U);
    DL_UART_enable(SERIAL_DATA_PLOT_INST);
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    commissioning_stage("boot: driver disabled");
}
/* Revision 24: hand the motor to TI's own Hall FOC state machine (application.h, hall_sen_foc_app.c) for a
 * bounded time, under the same pre-enable guards as the native tests. phase 0: TI Hall calibration
 * (flagStartHallCal); phase 1: TI Hall FOC run toward speedRefHz. The ISR runs TI's control; this loop runs TI's
 * 1 ms low-priority state machine, watches nFAULT, comparator trips, raw phase currents, the bus and Hall, and
 * stops on the first problem or the time cap. Driver off and the TI run flag cleared before printing. */
extern HallCal_Data_t hallSensorData;
extern volatile uint32_t frlArmed, frlIsrSinceBg, frlStarved, frlIsrMaxCycles, frlIsrCount;
static uint32_t fbits(float f){uint32_t u;memcpy(&u,&f,4);return u;}
static void hall_data_dump(const char *tag){ /* raw IEEE bits so a NaN or Inf cannot hide behind an integer cast */
    text(tag);kv("spd_bits=",(int32_t)fbits(hallData_M1.speedHall_Hz));kv("raw_bits=",(int32_t)fbits(hallData_M1.speedHall_raw_Hz));
    kv("ang_bits=",(int32_t)fbits(hallData_M1.angleHall_rad));kv("filt_bits=",(int32_t)fbits(hallData_M1.speedFilter_rps));
    kv("dir_bits=",(int32_t)fbits(hallData_M1.direction));kv("fault=",(int32_t)hallData_M1.flagHallFault);kv("state=",(int32_t)hallData_M1.hallState);
    kv("noedge=",(int32_t)hallData_M1.noEdgeIsrCnt);kv("cap=",(int32_t)hallData_M1.capturePeriod_cnt);
    kv("fwd_ptr_ok=",(int32_t)(hallConfig_M1.calAngleFwdTablePtr==hallSensorData.calAngleFwd_rad));text("\r\n");}
/* ilimOff: drive the DRV8376 ILIMIT DAC (PA4, CMPSS2 low DAC) to full scale. The data sheet: ILIMIT at VREF disables
 * the cycle-by-cycle current limit, which the H variant reports on nFAULT. The driver's fixed OCP and thermal shutdown,
 * the comparator trip, the 900 mV phase-current guard and the time cap stay on. */
static void bounded_ti_foc(unsigned phase, float speedRefHz, uint32_t ms, bool ilimOff) {
    MCPWM_Regs *pwm=(MCPWM_Regs *)halFOCConfig_M1.pwmCfg.pwmHandle;
    const uint32_t trips=DL_MCPWM_TZ_SIGNAL_OST2|DL_MCPWM_TZ_SIGNAL_OST3;
    uint32_t savedPin=IOMUX->SECCFG.PINCM[IOMUX_PINCM_PA4];
    uint16_t savedLow=DL_CMPSSLITE_getDACValueLow(CMPSS2);
    off(); DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM_PA4);
    DL_CMPSSLITE_setDACValueLow(CMPSS2,ilimOff?4095U:2048U);
    DL_SYSCTL_enableDACOUT(DL_SYSCTL_DACOUTEN_CMPSS2_DACL); DEVICE_DELAY_MS(5);
    uint32_t reference=auxiliary_pull_probe(true),vm,a,b,c;sample(&vm,&a,&b,&c);
    unsigned initialHall=read_hall();
    if(phase>1U || ms>(phase==0U ? 700000U : 12000U) || initialHall==0U || initialHall==7U || (ilimOff ? (reference<3150U) : (reference<1670U || reference>1730U)) || pwm_pads() ||
       !DL_GPIO_readPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT,MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN) ||
       !motorVars_M1.statusVars.flagOffsetCalibDone || !commissioning_ready(fault(),vm,a,b,c) ||
       (DL_MCPWM_getTripZoneFlagStatus(pwm)&trips) || motorVars_M1.outVars.faultMtrNow.all!=0U) {
        restore_reference(savedPin,savedLow);text("ti rejected: preflight\r\n");print_status();return;
    }
    const uint32_t baseline[3]={a,b,c};
    uint32_t peak[3]={0},reason=0,minVm=vm,maxVm=vm,lastLog=0;
    int net=0;unsigned h=initialHall,candidate=h,stable=0,edges=0,skips=0,maxState=0;
    static struct {uint32_t ms;int state,cal,spd,id,iq,idr,iqr,vd,vq,hall,net,hs,ts,ah,af;uint32_t flt,cap;} rows[240];unsigned count=0;
    hall_data_dump("hall_before ");
    motorVars_M1.inputCtrl.speedRef_Hz=speedRefHz;
    motorVars_M1.inputCtrl.flagStartHallCal=(phase==0U);
    for(unsigned i=0;i<3;i++) FOC_DATA.pwmData.Vabc_pu.value[i]=0.0f;
    HAL_writePWMData(&halFOCConfig_M1.pwmCfg,&FOC_DATA.pwmData);
    DEVICE_DELAY_US(200);
    DL_GPIO_clearPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT,MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN);
    frlStarved=0U;frlIsrMaxCycles=0U;frlIsrSinceBg=0U;frlArmed=1U;
    motorVars_M1.inputCtrl.flagEnableRunAndIdentify=true;
    uint32_t begin=DWT->CYCCNT,last=begin,age=0U,frac=0U;
    while(!reason) {
        frlIsrSinceBg=0U;
        if(frlStarved){reason=10;break;} /* the ISR dead-man stopped the drive */
        /* Revision 28: the 32-bit cycle counter wraps every 21.47 s at 200 MHz, so elapsed time is accumulated from
         * per-pass deltas; before this, a cap longer than 21 s never fired and printed durations were modulo 21.47 s. */
        uint32_t nowc=DWT->CYCCNT; frac+=nowc-last; last=nowc; while(frac>=200000U){frac-=200000U;++age;} /* ms */
        if(age>=ms) break;
        if(fault() || (DL_MCPWM_getTripZoneFlagStatus(pwm)&trips)) {reason=1;break;}
        if(motorVars_M1.outVars.faultMtrNow.all!=0U) {reason=9;break;}
        if(phase==0U && hallSensorData.state==HALL_CAL_DONE && hallSensorData.flagCalDone) {reason=11;break;} /* calibration finished: stop here, do not roll into a run */
        if(!DL_UART_isRXFIFOEmpty(SERIAL_DATA_PLOT_INST) && DL_UART_receiveData(SERIAL_DATA_PLOT_INST)=='s') {reason=8;break;}
        if(systemVars.timerCntFOCCtrlISR>LOW_PRIORITY_WAIT_TIME_1ms) {systemVars.timerCntFOCCtrlISR=0;FOC_runLowPriorityStateMachine(&motorVars_M1);}
        uint32_t v[3]={raw_mv(DL_ADC_readResult(MTR1_IU_SEN_ADC_RESULT_INST,MTR1_IU_SEN)),raw_mv(DL_ADC_readResult(MTR1_IV_SEN_ADC_RESULT_INST,MTR1_IV_SEN)),raw_mv(DL_ADC_readResult(MTR1_IW_SEN_ADC_RESULT_INST,MTR1_IW_SEN))};
        for(unsigned i=0;i<3;i++){int d=(int)baseline[i]-(int)v[i];unsigned ad=d<0?-d:d;if(ad>peak[i])peak[i]=ad;if(ad>900U)reason=6;}
        uint32_t bus=raw_mv(DL_ADC_readResult(MTR1_VDC_SEN_ADC_RESULT_INST,MTR1_VDC_SEN))*14904U/604U;
        if(bus<9000U || bus>15000U) reason=7;
        if(bus<minVm)minVm=bus;if(bus>maxVm)maxVm=bus;
        unsigned observed=read_hall();
        if(observed==candidate)++stable;else {candidate=observed;stable=1;}
        if(stable>=3U && (candidate==0U || candidate==7U)){reason=2;break;}
        if(stable>=3U && candidate!=h){int step=commissioning_hall_step(h,candidate);net+=step;++edges;if(!step && ++skips>=3U){reason=3;break;}h=candidate;}
        unsigned st=(unsigned)motorVars_M1.outVars.motorState;if(st>maxState)maxState=st;
        { static uint32_t hb=0U; if(age==0U)hb=0U; if(age>=hb){hb=age+2000U; /* revision 29: live heartbeat */
            kv("hb_ms=",age);kv("state=",(int32_t)st);kv("cal=",(int32_t)hallSensorData.state);kv("found=",(int32_t)hallSensorData.statesFound);
            kv("hall=",(int32_t)h);kv("net=",net);kv("spd_cHz=",(int32_t)(motorVars_M1.outVars.speed_Hz*100.0f));
            kv("id=",(int32_t)(motorVars_M1.outVars.Idq_in_A.value[0]*1000.0f));kv("iq=",(int32_t)(motorVars_M1.outVars.Idq_in_A.value[1]*1000.0f));
            kv("flt=",(int32_t)motorVars_M1.outVars.faultMtrNow.all);kv("isr_max_us=",(int32_t)(frlIsrMaxCycles/200U));text("\r\n");} }
        if(count<240U && (count==0U || age>=lastLog+(phase==0U ? 7500U : 50U))) {
            lastLog=age;rows[count].ms=age;rows[count].state=(int)st;rows[count].cal=(int)hallSensorData.state;
            rows[count].spd=(int)(motorVars_M1.outVars.speed_Hz*100.0f);
            rows[count].id=(int)(motorVars_M1.outVars.Idq_in_A.value[0]*1000.0f);rows[count].iq=(int)(motorVars_M1.outVars.Idq_in_A.value[1]*1000.0f);
            rows[count].idr=(int)(motorVars_M1.outVars.IdqRef_A.value[0]*1000.0f);rows[count].iqr=(int)(motorVars_M1.outVars.IdqRef_A.value[1]*1000.0f);
            rows[count].vd=(int)(motorVars_M1.outVars.Vdq_out_V.value[0]*1000.0f);rows[count].vq=(int)(motorVars_M1.outVars.Vdq_out_V.value[1]*1000.0f);
            rows[count].hall=(int)h;rows[count].net=net;rows[count].flt=motorVars_M1.outVars.faultMtrNow.all;
            rows[count].hs=(int)(hallData_M1.speedHall_Hz*100.0f);rows[count].cap=hallData_M1.capturePeriod_cnt;rows[count].ts=(int)hallData_M1.hallState;
            rows[count].ah=(int)(hallData_M1.angleHall_rad*57.29578f);rows[count].af=(int)(motorVars_M1.outVars.angleFOC_rad*57.29578f);++count;
        }
    }
    frlArmed=0U;off();motorVars_M1.inputCtrl.flagStartHallCal=false;motorVars_M1.inputCtrl.speedRef_Hz=0.0f;
    uint32_t stopAt=DWT->CYCCNT;
    while((uint32_t)(DWT->CYCCNT-stopAt)<4000000U) { /* 20 ms of low-priority ticks so TI returns to idle */
        if(systemVars.timerCntFOCCtrlISR>LOW_PRIORITY_WAIT_TIME_1ms){systemVars.timerCntFOCCtrlISR=0;FOC_runLowPriorityStateMachine(&motorVars_M1);}
    }
    off();uint32_t elapsed=age*1000U;restore_reference(savedPin,savedLow);
    hall_data_dump("hall_after ");
    kv("ti_phase=",phase);kv("speed_ref_cHz=",(int32_t)(speedRefHz*100.0f));kv("elapsed_us=",elapsed);kv("reason=",reason);kv("net_sectors=",net);kv("edges=",edges);kv("skips=",skips);
    kv("max_state=",maxState);kv("end_state=",(int32_t)motorVars_M1.outVars.motorState);kv("hallcal_state=",(int32_t)hallSensorData.state);
    kv("hallcal_done=",(int32_t)hallSensorData.flagCalDone);kv("hallcal_valid=",(int32_t)hallSensorData.flagCalValid);kv("hallcal_states=",(int32_t)hallSensorData.statesFound);
    kv("fault_reg=",(int32_t)motorVars_M1.outVars.faultMtrNow.all);kv("starved=",(int32_t)frlStarved);kv("isr_max_us=",(int32_t)(frlIsrMaxCycles/200U));kv("min_vm_mV=",minVm);kv("max_vm_mV=",maxVm);text("\r\n");
    for(unsigned i=0;i<3;i++){kv("physical_phase=",i);kv("peak_delta_mV=",peak[i]);text("\r\n");}
    if(phase==0U){for(unsigned s=1;s<=6;s++){kv("hall_state=",s);kv("fwd_cdeg=",(int32_t)(hallSensorData.calAngleFwd_rad[s]*5729.578f));kv("rev_cdeg=",(int32_t)(hallSensorData.calAngleRev_rad[s]*5729.578f));text("\r\n");}}
    for(unsigned i=0;i<count;i++){kv("t_ms=",rows[i].ms);kv("state=",rows[i].state);kv("hallcal=",rows[i].cal);kv("speed_cHz=",rows[i].spd);kv("id_mA=",rows[i].id);kv("iq_mA=",rows[i].iq);
        kv("id_ref_mA=",rows[i].idr);kv("iq_ref_mA=",rows[i].iqr);kv("vd_mV=",rows[i].vd);kv("vq_mV=",rows[i].vq);kv("hall=",rows[i].hall);kv("net=",rows[i].net);kv("fault=",(int32_t)rows[i].flt);
        kv("ti_hall=",rows[i].ts);kv("hall_spd_cHz=",rows[i].hs);kv("cap_cnt=",(int32_t)rows[i].cap);kv("ang_hall_deg=",rows[i].ah);kv("ang_foc_deg=",rows[i].af);text("\r\n");}
    print_status();
}

void commissioning_loop(void) {
    off();
    commissioning_stage("I=100ms TI current steps C=6s TI current-controlled field B=100ms native zero D=100ms small static voltage E=6s sine ramp R=2s reverse10pct L=6s forward10pct T=6s duty8-15-8 X=Hall-loss injection J=2s duty8 K=2s duty12 H=haptic knob 20s O=2s slow open-loop F/S/G=2s current-limited sixstep at5/10/15pct 0..5=50us six line vectors m=50us 3x AC u/j=20/50us pulse with measured DAC limit k=1.7V DAC disabled-drive check e/g=disabled B/C first t=aux pull with driver disabled h=sleep response w=aux weak-pull d=disabled input trace v=status c=sleep/wake s=stop z=100ms all-inputs-low enable p/q/r=disabled; no run command");
    print_status();
    for (;;) {
        int ch = DL_UART_isRXFIFOEmpty(SERIAL_DATA_PLOT_INST) ? -1 : DL_UART_receiveData(SERIAL_DATA_PLOT_INST);
        if (ch == 'Y') bounded_ti_foc(0U, 0.0f, 660000U, false); /* TI Hall calibration as shipped: 2 deg per 400 ms, about 10 min; 11 min cap */
        if (ch == 'N') bounded_ti_foc(1U, 10.0f, 12000U, false); /* TI Hall FOC, 10 Hz ref, 12 s: TI aligns 5 s at 0.4 A, ramps open loop to 30 Hz (OL_SPEED_THR_Hz) at 10 Hz/s, then closes the loop */
        if (ch == 'P') bounded_ti_foc(1U, 20.0f, 12000U, false); /* TI Hall FOC, 20 Hz ref, 12 s */
        if (ch == 'U') bounded_ti_foc(1U, 10.0f, 12000U, true);  /* as N with the DRV8376 cycle-by-cycle current limit off (ILIMIT at full scale) */
        if (ch == 'I') bounded_native_pwm(3U);
        if (ch == 'C') bounded_native_pwm(4U);
        if (ch == 'B') bounded_native_pwm(0U);
        if (ch == 'D') bounded_native_pwm(1U);
        if (ch == 'E') bounded_native_pwm(2U);
        if (ch == 'O') bounded_sixstep(10U,true,1,2000U,0U);
        if (ch == 'F') bounded_sixstep(5U,false,1,2000U,0U);
        if (ch == 'S') bounded_sixstep(10U,false,1,2000U,0U);
        if (ch == 'G') bounded_sixstep(15U,false,1,2000U,0U);
        if (ch == 'R') bounded_sixstep(10U,false,-1,2000U,0U);
        if (ch == 'L') bounded_sixstep(10U,false,1,6000U,0U);
        if (ch == 'T') bounded_sixstep(8U,false,1,6000U,1U);
        if (ch == 'X') bounded_sixstep(10U,false,1,2000U,2U);
        if (ch == 'J') bounded_sixstep(8U,false,1,2000U,0U);
        if (ch == 'K') bounded_sixstep(12U,false,1,2000U,0U);
        if (ch == 'H') bounded_haptic(); /* haptic knob, 20 s cap: see the comment above bounded_haptic */
        if (ch == 'v' || ch == '?') print_status();
        if (ch == 'd') disabled_trace(1U);
        if (ch == 'e') disabled_trace(4U);
        if (ch == 'g') disabled_trace(16U);
        if (ch == 't') {
            /* Source-impedance/fault coupling test only: never lower DRVOFF.
             * Restore the auxiliary pin before accepting another command. */
            off(); DL_SYSCTL_disableDACOUT();
            uint32_t savedPin=IOMUX->SECCFG.PINCM[IOMUX_PINCM_PA4];
            IOMUX->SECCFG.PINCM[IOMUX_PINCM_PA4]=IOMUX_PINCM_PC_UNCONNECTED | IOMUX_PINCM_PIPU_ENABLE;
            DEVICE_DELAY_MS(5); disabled_trace(1U);
            off(); IOMUX->SECCFG.PINCM[IOMUX_PINCM_PA4]=savedPin;
            text("aux pull restored; driver disabled\r\n");
        }
        if (ch == 'w') (void)auxiliary_pull_probe(false);
        if (ch == 'k') {
            off();
            uint32_t savedPin=IOMUX->SECCFG.PINCM[IOMUX_PINCM_PA4];
            uint16_t savedLow=DL_CMPSSLITE_getDACValueLow(CMPSS2);
            DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM_PA4);
            DL_CMPSSLITE_setDACValueLow(CMPSS2, 2048U); /* Measured 2110 -> 1.751 V; target about 1.700 V. */
            DL_SYSCTL_enableDACOUT(DL_SYSCTL_DACOUTEN_CMPSS2_DACL);
            DEVICE_DELAY_MS(5);
            uint32_t measured=auxiliary_pull_probe(true);
            kv("dac_code=", DL_CMPSSLITE_getDACValueLow(CMPSS2)); kv("dac_measured_mV=", measured); text("\r\n");
            if (measured>=1670U && measured<=1730U) disabled_trace(1U);
            else text("DAC voltage outside diagnostic range; no input trace\r\n");
            off(); DL_SYSCTL_disableDACOUT(); DL_CMPSSLITE_setDACValueLow(CMPSS2, savedLow);
            IOMUX->SECCFG.PINCM[IOMUX_PINCM_PA4]=savedPin;
            text("DAC disabled; bridge disabled\r\n");
        }
        if (ch == 'h') {
            off(); text("sleep response before: "); print_status();
            DL_GPIO_clearPins(MTR1_GATE_DRIVER_NSLEEP_GPIO_PORT, MTR1_GATE_DRIVER_NSLEEP_GPIO_PIN);
            DEVICE_DELAY_MS(10); text("sleep response asleep: "); print_status();
            DL_GPIO_setPins(MTR1_GATE_DRIVER_NSLEEP_GPIO_PORT, MTR1_GATE_DRIVER_NSLEEP_GPIO_PIN);
            DEVICE_DELAY_MS(6); text("sleep response awake: "); print_status();
        }
        if (ch == 's') { off(); print_status(); }
        if (ch == 'c') {
            off();
            DL_GPIO_clearPins(MTR1_GATE_DRIVER_NSLEEP_GPIO_PORT, MTR1_GATE_DRIVER_NSLEEP_GPIO_PIN);
            DEVICE_DELAY_MS(10);
            DL_GPIO_setPins(MTR1_GATE_DRIVER_NSLEEP_GPIO_PORT, MTR1_GATE_DRIVER_NSLEEP_GPIO_PIN);
            DEVICE_DELAY_MS(6);
            print_status();
        }
        if (ch == 'p' || ch == 'q' || ch == 'r') {
            text("legacy pulse disabled: use measured-reference commands\r\n"); continue;
        }
        if (ch == 'u' || ch == 'j' || ch == 'm' || (ch >= '0' && ch <= '5')) {
            const uint32_t pulseCycles = ch == 'u' ? 4000U : 10000U;
            const bool usingDac = true;
            const bool threeX = ch == 'm';
            const VOLTAGE_VECTOR vectors[] = {VOLTAGE_VECTOR_AB,VOLTAGE_VECTOR_AC,VOLTAGE_VECTOR_BC,VOLTAGE_VECTOR_BA,VOLTAGE_VECTOR_CA,VOLTAGE_VECTOR_CB};
            const uint32_t masks[] = {9U,33U,36U,6U,18U,24U};
            unsigned vectorIndex = (ch >= '0' && ch <= '5') ? (unsigned)(ch-'0') : 1U;
            uint32_t savedPin=IOMUX->SECCFG.PINCM[IOMUX_PINCM_PA4];
            uint16_t savedLow=DL_CMPSSLITE_getDACValueLow(CMPSS2);
            const uint32_t expectedPads = threeX ? 35U : masks[vectorIndex];
            uint32_t vm, a, b, c; off(); sample(&vm, &a, &b, &c);
            if (usingDac) {
                DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM_PA4);
                DL_CMPSSLITE_setDACValueLow(CMPSS2, 2048U);
                DL_SYSCTL_enableDACOUT(DL_SYSCTL_DACOUTEN_CMPSS2_DACL);
                DEVICE_DELAY_MS(5);
                uint32_t measured=auxiliary_pull_probe(true);
                if (measured<1670U || measured>1730U) {
                    restore_reference(savedPin,savedLow);
                    text("pulse rejected: measured DAC reference out of range\r\n"); continue;
                }
                sample(&vm, &a, &b, &c);
            }
            MCPWM_Regs *pwm = (MCPWM_Regs *)halFOCConfig_M1.pwmCfg.pwmHandle;
            const uint32_t faultTrips = DL_MCPWM_TZ_SIGNAL_OST2 | DL_MCPWM_TZ_SIGNAL_OST3;
            if (!motorVars_M1.statusVars.flagOffsetCalibDone || pwm_pads() != 0U ||
                !DL_GPIO_readPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT, MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN) ||
                !commissioning_ready(fault(), vm, a, b, c) ||
                (DL_MCPWM_getTripZoneFlagStatus(pwm) & faultTrips)) {
                if (usingDac) restore_reference(savedPin,savedLow);
                text("pulse rejected: fault/calibration/feedback/pads/trip\r\n"); print_status(); continue;
            }
            __disable_irq();
            DL_MCPWM_disableADCTrigger(pwm, DL_MCPWM_SOC_A);
            uint32_t drainStart = DWT->CYCCNT;
            while (DL_ADC_isBusy(ADC2) &&
                   (uint32_t)(DWT->CYCCNT-drainStart) < 10000U) {}
            if (DL_ADC_isBusy(ADC2)) {
                off(); DL_MCPWM_enableADCTrigger(pwm, DL_MCPWM_SOC_A); __enable_irq();
                if (usingDac) restore_reference(savedPin,savedLow);
                text("pulse rejected: ADC busy\r\n"); continue;
            }
            DL_ADC_clearInterruptStatus(ADC2, DL_ADC_INT_NUMBER1);
            /* All inputs were forced low by off(); select one line vector.
             * Release MOTOR_STOP only. Never clear driver/comparator faults. */
            HAL_forceVoltageSector(vectors[vectorIndex], &halFOCConfig_M1.pwmCfg);
            if (threeX) DL_MCPWM_setActionQualifierSWAction(pwm, DL_MCPWM_AQ_OUTPUT_1B, DL_MCPWM_AQ_SW_CONTINUOUS_HIGH);
            DL_XBAR_setInputXBAR(DL_XBAR_INPUT1, DL_XBAR_DRIVE_HIGH);
            DL_MCPWM_clearTripZoneFlagStatus(pwm, DL_MCPWM_TZ_SIGNAL_OST1);
            DL_MCPWM_clearInterrupt(pwm, DL_MCPWM_INT_TZ_OST);
            /* Allow peripheral-to-pad propagation while DRVOFF remains high. */
            DL_Common_delayCycles(2000U);
            uint32_t armedPads = pwm_pads();
            uint32_t force3 = pwm->PWM3_AQSFRC;
            uint32_t pin25 = IOMUX->SECCFG.PINCM[IOMUX_PINCM_PA25];
            int seenFault = 0; uint32_t seenTrip = 0, rawC = 0, rawA = 0, rawB = 0, rawVA = 0, rawVB = 0, rawVC = 0; int sampled = 0, sampleValid = 0; uint32_t sampleUs = 0;
            uint32_t start = DWT->CYCCNT, elapsed = 0;
            if (armedPads == expectedPads) {
                HAL_forcePWMLow(&halFOCConfig_M1.pwmCfg);
                DL_Common_delayCycles(2000U);
                if (pwm_pads() == 0U) {
                    DL_GPIO_clearPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT, MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN);
                    uint32_t settleStart = DWT->CYCCNT;
                    while ((uint32_t)(DWT->CYCCNT-settleStart) < 200000U) {
                        if (fault()) { seenFault = 1; break; }
                    }
                } else seenFault = 1;
                start = DWT->CYCCNT;
                if (!seenFault) {
                    HAL_forceVoltageSector(vectors[vectorIndex], &halFOCConfig_M1.pwmCfg);
                    if (threeX) DL_MCPWM_setActionQualifierSWAction(pwm, DL_MCPWM_AQ_OUTPUT_1B, DL_MCPWM_AQ_SW_CONTINUOUS_HIGH);
                }
                while (!seenFault && (elapsed = (uint32_t)(DWT->CYCCNT-start)) < pulseCycles) {
                    seenFault = fault();
                    seenTrip = DL_MCPWM_getTripZoneFlagStatus(pwm) & faultTrips;
                    if (seenFault || seenTrip) break;
                    if (!sampled && elapsed >= pulseCycles/2U) {
                        DL_ADC_forceSequencer(MTR1_VDC_SEN_ADC_INST, DL_ADC_SEQ_NUMBER1);
                        DL_ADC_forceSequencer(ADC1, DL_ADC_SEQ_NUMBER1);
                        DL_ADC_forceSequencer(ADC2, DL_ADC_SEQ_NUMBER1);
                        sampled = 1;
                    }
                    /* Freshness depends on ADC2 completing 3 longer conversions after
                     * ADC0/1 complete 2 shorter ones. Recheck on SysConfig changes. */
                    if (sampled && !sampleValid &&
                        DL_ADC_getRawInterruptStatus(ADC2, DL_ADC_INT_NUMBER1)) {
                        rawC = DL_ADC_readResult(MTR1_IW_SEN_ADC_RESULT_INST, MTR1_IW_SEN);
                        rawA = DL_ADC_readResult(MTR1_IU_SEN_ADC_RESULT_INST, MTR1_IU_SEN);
                        rawB = DL_ADC_readResult(MTR1_IV_SEN_ADC_RESULT_INST, MTR1_IV_SEN);
                        rawVA = DL_ADC_readResult(MTR1_VU_SEN_ADC_RESULT_INST, MTR1_VU_SEN);
                        rawVB = DL_ADC_readResult(MTR1_VV_SEN_ADC_RESULT_INST, MTR1_VV_SEN);
                        rawVC = DL_ADC_readResult(MTR1_VW_SEN_ADC_RESULT_INST, MTR1_VW_SEN);
                        sampleUs = (uint32_t)(DWT->CYCCNT-start)/200U;
                        sampleValid = 1;
                    }
                }
                off(); /* No serial printing or interrupt can delay this stop. */
            } else off();
            DL_MCPWM_enableADCTrigger(pwm, DL_MCPWM_SOC_A);
            __enable_irq();
            if (usingDac) restore_reference(savedPin,savedLow);
            kv("reference_dac=", usingDac); kv("requested_us=", pulseCycles/200U); kv("encoding=", threeX ? 3 : 6); kv("vector=",vectorIndex);
            kv("a_before_mV=",a); kv("b_before_mV=",b); kv("vb_raw=",rawVB); kv("vc_raw=",rawVC); kv("a_raw=", rawA); kv("b_raw=", rawB); kv("va_raw=", rawVA);
            kv("force3=", force3); kv("pin25=", pin25);
            kv("pulse_pads=", armedPads); kv("pulse_fault=", seenFault);
            kv("pulse_trip=", seenTrip); kv("pulse_us=", elapsed/200U);
            kv("sample_valid=", sampleValid && !seenFault && !seenTrip); kv("sample_us=", sampleUs);
            kv("sample_started=", sampled); kv("c_before_mV=", c); kv("c_after_raw=", rawC);
            text("\r\n"); DEVICE_DELAY_MS(1); print_status();
        }
        if (ch == 'z') {
            uint32_t vm, a, b, c; off(); sample(&vm, &a, &b, &c);
            if (pwm_pads() != 0U || !DL_GPIO_readPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT, MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN) ||
                !motorVars_M1.statusVars.flagOffsetCalibDone || !commissioning_ready(fault(), vm, a, b, c)) {
                text("enable rejected: fault/calibration/feedback\r\n"); print_status(); continue;
            }
            /* PWM remains tripped by MOTOR_STOP; do not clear any trip flags. */
            __disable_irq(); /* Bound this test independently of the FOC ISR. */
            uint32_t start = DWT->CYCCNT; int seen_fault = 0;
            DL_GPIO_clearPins(MTR1_GATE_DRIVER_DRVOFF_GPIO_PORT, MTR1_GATE_DRIVER_DRVOFF_GPIO_PIN);
            while ((uint32_t)(DWT->CYCCNT-start) < 20000000U) {
                if (fault()) { seen_fault = 1; break; }
                if (!DL_UART_isRXFIFOEmpty(SERIAL_DATA_PLOT_INST) && DL_UART_receiveData(SERIAL_DATA_PLOT_INST) == 's') break;
            }
            off();
            __enable_irq();
            kv("enable_fault=", seen_fault); kv("elapsed_us=", (DWT->CYCCNT-start)/200U); text("\r\n"); print_status();
        }
    }
}
