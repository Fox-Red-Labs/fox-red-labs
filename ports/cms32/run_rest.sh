#!/bin/bash
cd "$(dirname "$0")"
run() { ./run.sh "$1" "${2:-6}" "${3:-19200}" > /dev/null 2>&1; echo "$1: $(grep -c '^  | ' runs/${1//\//_}.txt) serial lines, DOUT $(grep '^DOUT' runs/${1//\//_}.txt | awk '{print $NF}' | tr '\n' ' ')"; }
run wwdg 4
run itIntervalTimer 6 19200; run flashEraseWrite 6 19200
run uart/uartSendReceive 3 115200; run usart/usart_printf 4 115200; run usart/usart_dma 4 9600
run timer/timer1 4; run timer/timer3 4; run timer/timer14_timing 4; run timer/timer16 4; run timer/timer17 4; run timer/timer1_dma 4; run timer/timer14 4
run adc/adc_demo/adcSoftTrigger 6 19200; run adc/adc_demo/adcHardTrigNoWait 6 19200; run adc/adc_demo/adcSoftTrigger_dma 4 19200
run adc/adc_demo/adcTempSensor 5 19200; run adc/adc_demo/adcHardTrigNoWait_Scan4Channel 4 19200
run acmp/ACMP0 3; run acmp/ACMP1 3; run OP/OP0/code 3; run OP/OP1/code 3
run STB/sleep 4; run STB/deepsleep_itWakeup 14 9600; run STB/deepsleep 8
