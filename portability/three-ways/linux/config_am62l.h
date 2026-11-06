/* Three ways, Linux configuration for the TMDS62LEVM (AM62L EVM): which sysfs files the glue uses. */
#ifndef CONFIG_AM62L_H
#define CONFIG_AM62L_H
#define ADC_RAW "/sys/bus/iio/devices/iio:device0/in_voltage0_raw"  /* TI-am335x-adc, 12 bit, channel 0 */
#define LED_DIR "/sys/class/leds/am62-sk:green:heartbeat"            /* the board's green user LED */
#endif
