/* Three ways, Linux configuration for the BeagleY-AI (AM67A). Used with -DCONFIG_AM62L_H so glue_linux.c stays byte-identical. */
#define ADC_RAW "/sys/bus/iio/devices/iio:device0/in_voltage0_raw"  /* absent: the board has no ADC; the glue returns 0xFFFF */
#define LED_DIR "/sys/class/leds/ACT"                                 /* the board's green ACT LED */
